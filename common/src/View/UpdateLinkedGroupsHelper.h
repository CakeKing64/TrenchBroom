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

#pragma once

#include "FloatType.h"
#include "Result.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace TrenchBroom::Model
{
class EntityNode;
class GroupNode;
class Node;
} // namespace TrenchBroom::Model

namespace TrenchBroom::View
{
class MapDocumentCommandFacade;

/**
 * Checks whether the given vector of linked group can be updated consistently.
 *
 * The given linked groups can be updated consistently if no two of them are in the same
 * linked set.
 */
bool checkLinkedGroupsToUpdate(const std::vector<Model::GroupNode*>& changedLinkedGroups);

/**
 * Generate unique IDs for every entity in the given link set. For each of the given
 * groups, every entity at the same position in the node tree gets the same ID. Consider
 * the following example where we pass two linked groups A and B with identical
 * structures:
 *
 * GroupNode A             GroupNode B
 * - EntityNode A1         - EntityNode B1
 * - BrushNode A2          - BrushNode B2
 * - GroupNode A3          - GroupNode B3
 *   - BrushNode A3_1        - BrushNode B3_1
 *   - EntityNode A3_2       - EntityNode B3_2
 *
 * Given that A and B have the same link ID, then the returned map will contain the
 * following data:
 * - A1: some_unique_id
 * - B1: some_unique_id
 * - A3_2: some_other_unique_id
 * - B3_2: some_other_unique_id
 *
 * Note how the entities at the same positions in the subtrees such as A1 and B1 received
 * the same ID, and entities at different positions, such as A1 and A3_2 received
 * different IDs.
 *
 * The function returns nullopt if any of the passed groups' subtrees have a different
 * structure. The function requires that the given vector contains at least two group
 * nodes, and that all top level group nodes in the given vector have the same link ID.
 */
std::optional<std::unordered_map<const Model::EntityNode*, std::string>>
generateEntityLinkIds(const std::vector<Model::GroupNode*>& groupNodes);

/**
 * A helper class to add support for updating linked groups to commands.
 *
 * The class is initialized with a vector of group nodes whose changes should be
 * propagated to the members of their respective link sets. When applyLinkedGroupUpdates
 * is first called, a replacement node is created for each linked group that needs to be
 * updated, and these linked groups are replaced with their replacements. Calling
 * applyLinkedGroupUpdates replaces the replacement nodes with their original
 * corresponding groups again, effectively undoing the change.
 */
class UpdateLinkedGroupsHelper
{
private:
  using ChangedLinkedGroups = std::vector<Model::GroupNode*>;
  using LinkedGroupUpdates =
    std::vector<std::pair<Model::Node*, std::vector<std::unique_ptr<Model::Node>>>>;
  std::variant<ChangedLinkedGroups, LinkedGroupUpdates> m_state;

public:
  explicit UpdateLinkedGroupsHelper(ChangedLinkedGroups changedLinkedGroups);
  ~UpdateLinkedGroupsHelper();

  Result<void> applyLinkedGroupUpdates(MapDocumentCommandFacade& document);
  void undoLinkedGroupUpdates(MapDocumentCommandFacade& document);
  void collateWith(UpdateLinkedGroupsHelper& other);

private:
  Result<void> computeLinkedGroupUpdates(MapDocumentCommandFacade& document);
  static Result<LinkedGroupUpdates> computeLinkedGroupUpdates(
    const ChangedLinkedGroups& changedLinkedGroups, MapDocumentCommandFacade& document);

  void doApplyOrUndoLinkedGroupUpdates(MapDocumentCommandFacade& document);
};
} // namespace TrenchBroom::View
