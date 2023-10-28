/*
 Copyright (C) 2020 Kristian Duske

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

#include "UpdateLinkedGroupsHelper.h"

#include "Ensure.h"
#include "Error.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/ModelUtils.h"
#include "Model/Node.h"
#include "Model/WorldNode.h"
#include "Uuid.h"
#include "View/MapDocumentCommandFacade.h"

#include <kdl/overload.h>
#include <kdl/result.h>
#include <kdl/result_fold.h>
#include <kdl/vector_utils.h>

#include <algorithm>
#include <cassert>
#include <map>
#include <unordered_set>

namespace TrenchBroom::View
{
namespace
{
template <typename F>
bool visitNodesPerPosition(const std::vector<Model::Node*>& nodes, const F& f)
{
  if (nodes.empty())
  {
    return true;
  }

  if (!f(nodes))
  {
    return false;
  }

  const auto childCount = nodes.front()->childCount();
  if (!std::all_of(nodes.begin(), nodes.end(), [&](const auto& node) {
        return node->childCount() == childCount;
      }))
  {
    return false;
  }

  for (size_t i = 0; i < childCount; ++i)
  {
    auto toVisit = std::vector<Model::Node*>{};
    toVisit.reserve(nodes.size());

    for (const auto& node : nodes)
    {
      toVisit.push_back(node->children().at(i));
    }

    if (!visitNodesPerPosition(toVisit, f))
    {
      return false;
    }
  }

  return true;
}

// Order groups so that descendants will be updated before their ancestors
auto compareByAncestry(const Model::GroupNode* lhs, const Model::GroupNode* rhs)
{
  return rhs->isAncestorOf(lhs);
}

} // namespace

bool checkLinkedGroupsToUpdate(const std::vector<Model::GroupNode*>& changedLinkedGroups)
{
  const auto linkedGroupIds =
    kdl::vec_sort(kdl::vec_transform(changedLinkedGroups, [](const auto* groupNode) {
      return groupNode->group().linkedGroupId();
    }));

  return std::adjacent_find(std::begin(linkedGroupIds), std::end(linkedGroupIds))
         == std::end(linkedGroupIds);
}

std::optional<std::unordered_map<const Model::EntityNode*, std::string>>
generateEntityLinkIds(const std::vector<Model::GroupNode*>& groupNodes)
{
  ensure(groupNodes.size() > 1, "Generate entity links for at least two linked groups");

  const auto linkedGroupId = std::accumulate(
    std::next(groupNodes.begin()),
    groupNodes.end(),
    groupNodes.front()->group().linkedGroupId(),
    [](auto commonLinkedGroupId, const auto* groupNode) {
      const auto& currentLinkedGroupId = groupNode->group().linkedGroupId();
      return commonLinkedGroupId && commonLinkedGroupId == currentLinkedGroupId
               ? commonLinkedGroupId
               : std::nullopt;
    });
  ensure(linkedGroupId, "All groups are linked and share the same ID");

  auto result = std::unordered_map<const Model::EntityNode*, std::string>{};
  const auto success = visitNodesPerPosition(
    std::vector<Model::Node*>{groupNodes.begin(), groupNodes.end()},
    [&](const auto& nodes) {
      assert(!nodes.empty());
      if (dynamic_cast<Model::EntityNode*>(nodes.front()))
      {
        const auto entityLinkId = generateUuid();
        for (const auto* node : nodes)
        {
          const auto* entityNode = dynamic_cast<const Model::EntityNode*>(node);
          if (!entityNode)
          {
            return false;
          }

          result[entityNode] = entityLinkId;
        }
      }
      return true;
    });

  return success ? std::optional{std::move(result)} : std::nullopt;
}

UpdateLinkedGroupsHelper::UpdateLinkedGroupsHelper(
  ChangedLinkedGroups changedLinkedGroups)
  : m_state{kdl::vec_sort(std::move(changedLinkedGroups), compareByAncestry)}
{
}

UpdateLinkedGroupsHelper::~UpdateLinkedGroupsHelper() = default;

Result<void> UpdateLinkedGroupsHelper::applyLinkedGroupUpdates(
  MapDocumentCommandFacade& document)
{
  return computeLinkedGroupUpdates(document).transform(
    [&]() { doApplyOrUndoLinkedGroupUpdates(document); });
}

void UpdateLinkedGroupsHelper::undoLinkedGroupUpdates(MapDocumentCommandFacade& document)
{
  doApplyOrUndoLinkedGroupUpdates(document);
}

void UpdateLinkedGroupsHelper::collateWith(UpdateLinkedGroupsHelper& other)
{
  // Both helpers have already applied their changes at this point, so in both helpers,
  // m_linkedGroups contains pairs p where
  // - p.first is the group node to update
  // - p.second is a vector containing the group node's original children
  //
  // Let p_o be an update from the other helper. If p_o is an update for a linked group
  // node that was updated by this helper, then there is a pair p_t in this helper such
  // that p_t.first == p_o.first. In this case, we want to keep the old children of the
  // linked group node stored in this helper and discard those in the other helper. If p_o
  // is not an update for a linked group node that was updated by this helper, then we
  // will add p_o to our updates and remove it from the other helper's updates to prevent
  // the replaced node to be deleted with the other helper.

  auto& myLinkedGroupUpdates = std::get<LinkedGroupUpdates>(m_state);
  auto& theirLinkedGroupUpdates = std::get<LinkedGroupUpdates>(other.m_state);

  for (auto& [theirGroupNodeToUpdate, theirOldChildren] : theirLinkedGroupUpdates)
  {
    const auto myIt = std::find_if(
      std::begin(myLinkedGroupUpdates),
      std::end(myLinkedGroupUpdates),
      [theirGroupNodeToUpdate = theirGroupNodeToUpdate](const auto& p) {
        return p.first == theirGroupNodeToUpdate;
      });
    if (myIt == std::end(myLinkedGroupUpdates))
    {
      myLinkedGroupUpdates.emplace_back(
        theirGroupNodeToUpdate, std::move(theirOldChildren));
    }
  }
}

Result<void> UpdateLinkedGroupsHelper::computeLinkedGroupUpdates(
  MapDocumentCommandFacade& document)
{
  return std::visit(
    kdl::overload(
      [&](const ChangedLinkedGroups& changedLinkedGroups) {
        return computeLinkedGroupUpdates(changedLinkedGroups, document)
          .transform([&](auto&& linkedGroupUpdates) {
            m_state = std::forward<decltype(linkedGroupUpdates)>(linkedGroupUpdates);
          });
      },
      [](const LinkedGroupUpdates&) -> Result<void> { return kdl::void_success; }),
    m_state);
}

Result<UpdateLinkedGroupsHelper::LinkedGroupUpdates> UpdateLinkedGroupsHelper::
  computeLinkedGroupUpdates(
    const ChangedLinkedGroups& changedLinkedGroups, MapDocumentCommandFacade& document)
{
  if (!checkLinkedGroupsToUpdate(changedLinkedGroups))
  {
    return Error{"Cannot update multiple members of the same link set"};
  }

  const auto& worldBounds = document.worldBounds();
  return kdl::fold_results(
           kdl::vec_transform(
             changedLinkedGroups,
             [&](const auto* groupNode) {
               const auto groupNodesToUpdate = kdl::vec_erase(
                 Model::findLinkedGroups(
                   {document.world()}, *groupNode->group().linkedGroupId()),
                 groupNode);

               return Model::updateLinkedGroups(
                 *groupNode, groupNodesToUpdate, worldBounds);
             }))
    .and_then([&](auto nestedUpdateLists) -> Result<LinkedGroupUpdates> {
      return kdl::vec_flatten(std::move(nestedUpdateLists));
    });
}

void UpdateLinkedGroupsHelper::doApplyOrUndoLinkedGroupUpdates(
  MapDocumentCommandFacade& document)
{
  std::visit(
    kdl::overload(
      [](const ChangedLinkedGroups&) {},
      [&](LinkedGroupUpdates&& linkedGroupUpdates) {
        m_state = document.performReplaceChildren(std::move(linkedGroupUpdates));
      }),
    std::move(m_state));
}
} // namespace TrenchBroom::View
