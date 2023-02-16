/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ImageFileSystem.h"

#include "Ensure.h"
#include "IO/DiskFileSystem.h"
#include "IO/File.h"

#include <kdl/overload.h>

#include <cassert>
#include <memory>

namespace TrenchBroom
{
namespace IO
{

namespace
{
const Path& getName(const ImageEntry& entry)
{
  return std::visit([](const auto& x) -> const Path& { return x.name; }, entry);
}

bool isDirectory(const ImageEntry& entry)
{
  return std::visit(
    kdl::overload(
      [](const ImageDirectoryEntry&) { return true; },
      [](const ImageFileEntry&) { return false; }),
    entry);
}

bool isFile(const ImageEntry& entry)
{
  return !isDirectory(entry);
}

template <typename I>
auto findEntry(I begin, I end, const Path& name)
{
  return std::find_if(begin, end, [&](const auto& entry) {
    return getName(entry).compare(name, false) == 0;
  });
}

template <typename F>
auto withEntry(
  const Path& searchPath,
  const ImageEntry& currentEntry,
  const Path& currentPath,
  const F& f,
  decltype(f(
    std::declval<const ImageEntry&>(), std::declval<const Path&>())) defaultResult)
{
  if (searchPath.isEmpty())
  {
    return f(currentEntry, currentPath);
  }

  return std::visit(
    kdl::overload(
      [&](const ImageDirectoryEntry& directoryEntry) {
        const auto name = searchPath.firstComponent();
        const auto entryIt =
          findEntry(directoryEntry.entries.begin(), directoryEntry.entries.end(), name);

        return entryIt != directoryEntry.entries.end() ? withEntry(
                 searchPath.deleteFirstComponent(),
                 *entryIt,
                 currentPath + name,
                 f,
                 defaultResult)
                                                       : defaultResult;
      },
      [&](const ImageFileEntry&) { return defaultResult; }),
    currentEntry);
}

const ImageEntry* findEntry(const Path& path, const ImageEntry& parent)
{
  return withEntry(
    path,
    parent,
    Path{},
    [](const ImageEntry& entry, const Path&) { return &entry; },
    nullptr);
}

ImageDirectoryEntry& findOrCreateDirectory(const Path& path, ImageDirectoryEntry& parent)
{
  if (path.isEmpty())
  {
    return parent;
  }

  auto name = path.firstComponent();
  auto entryIt = findEntry(parent.entries.begin(), parent.entries.end(), name);
  if (entryIt != parent.entries.end())
  {
    return std::visit(
      kdl::overload(
        [&](ImageDirectoryEntry& directoryEntry) -> ImageDirectoryEntry& {
          return findOrCreateDirectory(path.deleteFirstComponent(), directoryEntry);
        },
        [&](ImageFileEntry&) -> ImageDirectoryEntry& {
          *entryIt = ImageDirectoryEntry{std::move(name), {}};
          return findOrCreateDirectory(
            path.deleteFirstComponent(), std::get<ImageDirectoryEntry>(*entryIt));
        }),
      *entryIt);
  }
  else
  {
    return findOrCreateDirectory(
      path.deleteFirstComponent(),
      std::get<ImageDirectoryEntry>(
        parent.entries.emplace_back(ImageDirectoryEntry{std::move(name), {}})));
  }
}
} // namespace

ImageFileSystemBase::ImageFileSystemBase(std::shared_ptr<FileSystem> next, Path path)
  : FileSystem{std::move(next)}
  , m_path{std::move(path)}
  , m_root{ImageDirectoryEntry{Path{}, {}}}
{
}

ImageFileSystemBase::~ImageFileSystemBase() = default;

void ImageFileSystemBase::reload()
{
  m_root = ImageDirectoryEntry{Path{}, {}};
  initialize();
}

void ImageFileSystemBase::initialize()
{
  try
  {
    doReadDirectory();
  }
  catch (const std::exception& e)
  {
    throw FileSystemException{
      "Could not initialize image file system '" + m_path.asString() + "': " + e.what()};
  }
}

void ImageFileSystemBase::addFile(const Path& path, GetImageFile getFile)
{
  auto& directoryEntry = findOrCreateDirectory(
    path.deleteLastComponent(), std::get<ImageDirectoryEntry>(m_root));

  auto name = path.lastComponent();
  if (const auto entryIt =
        findEntry(directoryEntry.entries.begin(), directoryEntry.entries.end(), name);
      entryIt != directoryEntry.entries.end())
  {
    *entryIt = ImageFileEntry{std::move(name), std::move(getFile)};
  }
  else
  {
    directoryEntry.entries.emplace_back(
      ImageFileEntry{std::move(name), std::move(getFile)});
  }
}

bool ImageFileSystemBase::doDirectoryExists(const Path& path) const
{
  const auto* entry = findEntry(path.makeCanonical(), m_root);
  return entry && isDirectory(*entry);
}

bool ImageFileSystemBase::doFileExists(const Path& path) const
{
  const auto* entry = findEntry(path.makeCanonical(), m_root);
  return entry && isFile(*entry);
}

std::vector<Path> ImageFileSystemBase::doGetDirectoryContents(const Path& path) const
{
  return withEntry(
    path.makeCanonical(),
    m_root,
    Path{},
    [](const ImageEntry& entry, const Path&) {
      return std::visit(
        kdl::overload(
          [&](const ImageDirectoryEntry& directoryEntry) -> std::vector<Path> {
            return kdl::vec_transform(
              directoryEntry.entries, [&](const auto& child) { return getName(child); });
          },
          [](const ImageFileEntry&) -> std::vector<Path> { return {}; }),
        entry);
    },
    std::vector<Path>{});
}

std::shared_ptr<File> ImageFileSystemBase::doOpenFile(const Path& path) const
{
  return withEntry(
    path.makeCanonical(),
    m_root,
    Path{},
    [](const ImageEntry& entry, const Path&) {
      return std::visit(
        kdl::overload(
          [&](const ImageDirectoryEntry&) -> std::shared_ptr<File> { return {}; },
          [](const ImageFileEntry& fileEntry) -> std::shared_ptr<File> {
            return fileEntry.getFile();
          }),
        entry);
    },
    {});
}

ImageFileSystem::ImageFileSystem(std::shared_ptr<FileSystem> next, Path path)
  : ImageFileSystemBase{std::move(next), std::move(path)}
  , m_file{std::make_shared<CFile>(m_path)}
{
  ensure(m_path.isAbsolute(), "path must be absolute");
}
} // namespace IO
} // namespace TrenchBroom
