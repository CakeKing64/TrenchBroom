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

#include "GroupNode.h"

#include "Ensure.h"
#include "Error.h"
#include "FloatType.h"
#include "Model/Brush.h"
#include "Model/BrushNode.h"
#include "Model/Entity.h"
#include "Model/EntityNode.h"
#include "Model/LayerNode.h"
#include "Model/ModelUtils.h"
#include "Model/NodeContents.h"
#include "Model/PatchNode.h"
#include "Model/PickResult.h"
#include "Model/TagVisitor.h"
#include "Model/Validator.h"
#include "Model/WorldNode.h"
#include "Uuid.h"

#include "kdl/zip_iterator.h"
#include <kdl/overload.h>
#include <kdl/parallel.h>
#include <kdl/range.h>
#include <kdl/result.h>
#include <kdl/result_fold.h>
#include <kdl/string_utils.h>
#include <kdl/vector_utils.h>

#include <vecmath/ray.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace TrenchBroom::Model
{
namespace
{
/**
 * Recursively collect the nodes to clone + transform, starting with the children of
 * `node`.
 * (`node` itself is skipped.)
 */
std::vector<const Node*> collectNodesToCloneAndTransform(const Node& node)
{
  auto result = std::vector<const Node*>{};

  std::function<void(const Node*)> collectNodes = [&](const Node* n) {
    result.push_back(n);
    for (auto* child : n->children())
    {
      collectNodes(child);
    }
  };

  for (auto* child : node.children())
  {
    collectNodes(child);
  }

  return result;
}

Result<std::unique_ptr<Node>> cloneAndTransformRecursive(
  const Node* nodeToClone,
  std::unordered_map<const Node*, NodeContents>& origNodeToTransformedContents,
  const vm::bbox3& worldBounds)
{
  // First, clone `n`, and move in the new (transformed) content which was
  // prepared for it above
  auto clone = nodeToClone->accept(kdl::overload(
    [](const WorldNode*) -> std::unique_ptr<Node> {
      ensure(false, "Linked group structure is valid");
    },
    [](const LayerNode*) -> std::unique_ptr<Node> {
      ensure(false, "Linked group structure is valid");
    },
    [&](const GroupNode* groupNode) -> std::unique_ptr<Node> {
      auto& group = std::get<Group>(origNodeToTransformedContents.at(groupNode).get());
      return std::make_unique<GroupNode>(std::move(group));
    },
    [&](const EntityNode* entityNode) -> std::unique_ptr<Node> {
      auto& entity = std::get<Entity>(origNodeToTransformedContents.at(entityNode).get());
      return std::make_unique<EntityNode>(std::move(entity));
    },
    [&](const BrushNode* brushNode) -> std::unique_ptr<Node> {
      auto& brush = std::get<Brush>(origNodeToTransformedContents.at(brushNode).get());
      return std::make_unique<BrushNode>(std::move(brush));
    },
    [&](const PatchNode* patchNode) -> std::unique_ptr<Node> {
      auto& patch =
        std::get<BezierPatch>(origNodeToTransformedContents.at(patchNode).get());
      return std::make_unique<PatchNode>(std::move(patch));
    }));

  if (!worldBounds.contains(clone->logicalBounds()))
  {
    return Error{"Updating a linked node would exceed world bounds"};
  }

  return kdl::fold_results(kdl::vec_transform(
                             nodeToClone->children(),
                             [&](const auto* childNode) {
                               return cloneAndTransformRecursive(
                                 childNode, origNodeToTransformedContents, worldBounds);
                             }))
    .transform([&](auto childClones) {
      for (auto& childClone : childClones)
      {
        clone->addChild(childClone.release());
      }

      return std::move(clone);
    });
}

/**
 * Given a node, clones its children recursively and applies the given transform.
 *
 * Returns a vector of the cloned direct children of `node`.
 */
Result<std::vector<std::unique_ptr<Node>>> cloneAndTransformChildren(
  const Node& node, const vm::bbox3& worldBounds, const vm::mat4x4& transformation)
{
  auto nodesToClone = collectNodesToCloneAndTransform(node);

  using TransformResult = Result<std::pair<const Node*, NodeContents>>;

  // In parallel, produce pairs { node pointer, transformed contents } from the nodes in
  // `nodesToClone`
  auto transformResults =
    kdl::vec_parallel_transform(nodesToClone, [&](const Node* nodeToTransform) {
      return nodeToTransform->accept(kdl::overload(
        [](const WorldNode*) -> TransformResult {
          ensure(false, "Linked group structure is valid");
        },
        [](const LayerNode*) -> TransformResult {
          ensure(false, "Linked group structure is valid");
        },
        [&](const GroupNode* groupNode) -> TransformResult {
          auto group = groupNode->group();
          group.transform(transformation);
          return std::make_pair(nodeToTransform, NodeContents{std::move(group)});
        },
        [&](const EntityNode* entityNode) -> TransformResult {
          auto entity = entityNode->entity();
          entity.transform(entityNode->entityPropertyConfig(), transformation);
          return std::make_pair(nodeToTransform, NodeContents{std::move(entity)});
        },
        [&](const BrushNode* brushNode) -> TransformResult {
          auto brush = brushNode->brush();
          return brush.transform(worldBounds, transformation, true)
            .and_then([&]() -> TransformResult {
              return std::make_pair(nodeToTransform, NodeContents{std::move(brush)});
            });
        },
        [&](const PatchNode* patchNode) -> TransformResult {
          auto patch = patchNode->patch();
          patch.transform(transformation);
          return std::make_pair(nodeToTransform, NodeContents{std::move(patch)});
        }));
    });

  return kdl::fold_results(std::move(transformResults))
    .or_else(
      [](const auto&) -> Result<std::vector<std::pair<const Node*, NodeContents>>> {
        return Error{"Failed to transform a linked node"};
      })
    .and_then(
      [&](auto origNodeAndTransformedContents)
        -> Result<std::vector<std::unique_ptr<Node>>> {
        // Move into map for easier lookup
        auto resultsMap = std::unordered_map<const Node*, NodeContents>{
          origNodeAndTransformedContents.begin(), origNodeAndTransformedContents.end()};
        origNodeAndTransformedContents.clear();

        // Do a recursive traversal of the input node tree again,
        // creating a matching tree structure, and move in the contents
        // we've transformed above.
        return kdl::fold_results(
          kdl::vec_transform(node.children(), [&](const auto* childNode) {
            return cloneAndTransformRecursive(childNode, resultsMap, worldBounds);
          }));
      });
}

template <typename T>
void preserveGroupNames(
  const std::vector<T>& clonedNodes, const std::vector<Model::Node*>& correspondingNodes)
{
  auto clIt = std::begin(clonedNodes);
  auto coIt = std::begin(correspondingNodes);
  while (clIt != std::end(clonedNodes) && coIt != std::end(correspondingNodes))
  {
    auto& clonedNode = *clIt;
    const auto* correspondingNode = *coIt;

    clonedNode->accept(kdl::overload(
      [](WorldNode*) {},
      [](LayerNode*) {},
      [&](GroupNode* clonedGroupNode) {
        if (
          const auto* correspondingGroupNode =
            dynamic_cast<const GroupNode*>(correspondingNode))
        {
          auto group = clonedGroupNode->group();
          group.setName(correspondingGroupNode->group().name());
          clonedGroupNode->setGroup(std::move(group));

          preserveGroupNames(
            clonedGroupNode->children(), correspondingGroupNode->children());
        }
      },
      [](EntityNode*) {},
      [](BrushNode*) {},
      [](PatchNode*) {}));

    ++clIt;
    ++coIt;
  }
}

void preserveEntityProperties(
  EntityNode& clonedEntityNode, const EntityNode& correspondingEntityNode)
{
  if (
    clonedEntityNode.entity().protectedProperties().empty()
    && correspondingEntityNode.entity().protectedProperties().empty())
  {
    return;
  }

  auto clonedEntity = clonedEntityNode.entity();
  const auto& correspondingEntity = correspondingEntityNode.entity();

  const auto allProtectedProperties = kdl::vec_sort_and_remove_duplicates(kdl::vec_concat(
    clonedEntity.protectedProperties(), correspondingEntity.protectedProperties()));

  clonedEntity.setProtectedProperties(correspondingEntity.protectedProperties());

  const auto entityPropertyConfig = clonedEntityNode.entityPropertyConfig();
  for (const auto& propertyKey : allProtectedProperties)
  {
    // this can change the order of properties
    clonedEntity.removeProperty(entityPropertyConfig, propertyKey);
    if (const auto* propertyValue = correspondingEntity.property(propertyKey))
    {
      clonedEntity.addOrUpdateProperty(entityPropertyConfig, propertyKey, *propertyValue);
    }
  }

  clonedEntityNode.setEntity(std::move(clonedEntity));
}

template <typename T>
void preserveEntityProperties(
  const std::vector<T>& clonedNodes, const std::vector<Node*>& correspondingNodes)
{
  auto clIt = std::begin(clonedNodes);
  auto coIt = std::begin(correspondingNodes);
  while (clIt != std::end(clonedNodes) && coIt != std::end(correspondingNodes))
  {
    auto& clonedNode =
      *clIt; // deduces either to std::unique_ptr<Node>& or Node*& depending on T
    const auto* correspondingNode = *coIt;

    clonedNode->accept(kdl::overload(
      [](WorldNode*) {},
      [](LayerNode*) {},
      [&](GroupNode* clonedGroupNode) {
        if (
          const auto* correspondingGroupNode =
            dynamic_cast<const GroupNode*>(correspondingNode))
        {
          preserveEntityProperties(
            clonedGroupNode->children(), correspondingGroupNode->children());
        }
      },
      [&](EntityNode* clonedEntityNode) {
        if (
          const auto* correspondingEntityNode =
            dynamic_cast<const EntityNode*>(correspondingNode))
        {
          preserveEntityProperties(*clonedEntityNode, *correspondingEntityNode);
        }
      },
      [](BrushNode*) {},
      [](PatchNode*) {}));

    ++clIt;
    ++coIt;
  }
}

template <typename F>
Result<void> visitNodesPerPosition(const Node& sourceNode, Node& targetNode, const F& f)
{
  return f(sourceNode, targetNode).and_then([&](const auto& recurse) -> Result<void> {
    if (recurse)
    {
      if (sourceNode.childCount() != targetNode.childCount())
      {
        return Error{"Inconsistent linked group structure"};
      }

      return kdl::fold_results(kdl::vec_transform(
        kdl::make_zip_range(sourceNode.children(), targetNode.children()),
        [&](auto& childPair) {
          auto& [sourceChild, targetChild] = childPair;
          return visitNodesPerPosition(*sourceChild, *targetChild, f);
        }));
    }

    return Result<void>{};
  });
}

template <typename N>
Result<bool> checkType(const Node& node, const bool successResult)
{
  return dynamic_cast<const N*>(&node)
           ? Result<bool>{successResult}
           : Result<bool>{Error{"Inconsistent linked group structure"}};
}

auto makeCopyLinkIds(const std::string& containingLinkedGroupId)
{
  return [&](const Node& sourceNode, Node& targetNode) {
    return sourceNode.accept(kdl::overload(
      [&](const WorldNode*) { return checkType<WorldNode>(targetNode, true); },
      [&](const LayerNode*) { return checkType<WorldNode>(targetNode, true); },
      [&](const GroupNode* sourceGroupNode) {
        // don't recurse into nested linked groups
        const auto& nestedLinkedGroupId = sourceGroupNode->group().linkedGroupId();
        const auto recurse =
          !nestedLinkedGroupId || nestedLinkedGroupId == containingLinkedGroupId;
        return checkType<GroupNode>(targetNode, recurse);
      },
      [&](const EntityNode* sourceEntityNode) {
        auto* targetEntityNode = dynamic_cast<EntityNode*>(&targetNode);
        if (!targetEntityNode)
        {
          return Result<bool>{Error{"Inconsistent linked group structure"}};
        }

        const auto& sourceLinkId = sourceEntityNode->entity().linkId();
        ensure(sourceLinkId, "Source entity has link ID");

        auto targetEntity = targetEntityNode->entity();
        targetEntity.setLinkId(*sourceLinkId);
        targetEntityNode->setEntity(std::move(targetEntity));

        return Result<bool>{true};
      },
      [&](const BrushNode*) { return checkType<BrushNode>(targetNode, false); },
      [&](const PatchNode*) { return checkType<PatchNode>(targetNode, false); }));
  };
}
} // namespace

Result<UpdateLinkedGroupsResult> updateLinkedGroups(
  const GroupNode& sourceGroupNode,
  const std::vector<Model::GroupNode*>& targetGroupNodes,
  const vm::bbox3& worldBounds)
{
  const auto& sourceGroup = sourceGroupNode.group();
  const auto [success, invertedSourceTransformation] =
    vm::invert(sourceGroup.transformation());
  if (!success)
  {
    return Error{"Group transformation is not invertible"};
  }

  const auto _invertedSourceTransformation = invertedSourceTransformation;
  const auto targetGroupNodesToUpdate =
    kdl::vec_erase(targetGroupNodes, &sourceGroupNode);
  return kdl::fold_results(
    kdl::vec_transform(targetGroupNodesToUpdate, [&](auto* targetGroupNode) {
      const auto transformation =
        targetGroupNode->group().transformation() * _invertedSourceTransformation;
      return cloneAndTransformChildren(sourceGroupNode, worldBounds, transformation)
        .transform([&](std::vector<std::unique_ptr<Node>>&& newChildren) {
          preserveGroupNames(newChildren, targetGroupNode->children());
          preserveEntityProperties(newChildren, targetGroupNode->children());

          return std::make_pair(
            static_cast<Node*>(targetGroupNode), std::move(newChildren));
        });
    }));
}

Result<void> copyLinkIds(const GroupNode& sourceNode, GroupNode& targetNode)
{
  return visitNodesPerPosition(
    sourceNode, targetNode, makeCopyLinkIds(*sourceNode.group().linkedGroupId()));
}

Result<void> setLinkIds(const std::vector<GroupNode*>& groupNodes)
{
  if (groupNodes.size() < 2)
  {
    return Error{"Link set must contain at least two groups"};
  }

  auto& sourceGroupNode = *groupNodes.front();
  const auto& sourceLinkedGroupId = sourceGroupNode.group().linkedGroupId();

  sourceGroupNode.accept(kdl::overload(
    [](const WorldNode*) {},
    [](const LayerNode*) {},
    [&](auto&& thisLambda, const GroupNode* groupNode) {
      // Don't recurse into nested linked groups
      const auto& nestedLinkedGroupId = groupNode->group().linkedGroupId();
      if (!nestedLinkedGroupId || nestedLinkedGroupId == sourceLinkedGroupId)
      {
        groupNode->visitChildren(thisLambda);
      }
    },
    [](auto&& thisLambda, EntityNode* entityNode) {
      auto entity = entityNode->entity();
      entity.setLinkId(generateUuid());
      entityNode->setEntity(std::move(entity));

      entityNode->visitChildren(thisLambda);
    },
    [](const BrushNode*) {},
    [](const PatchNode*) {}));

  return kdl::fold_results(kdl::vec_transform(
                             kdl::range{std::next(groupNodes.begin()), groupNodes.end()},
                             [&](auto* targetGroupNode) {
                               return copyLinkIds(sourceGroupNode, *targetGroupNode);
                             }))
    .or_else([&](auto e) {
      resetLinkIds(groupNodes);
      return Result<void>{std::move(e)};
    });
}

void resetLinkIds(const std::vector<GroupNode*>& groupNodes)
{
  Node::visitAll(
    groupNodes,
    kdl::overload(
      [](auto&& thisLambda, const WorldNode* worldNode) {
        worldNode->visitChildren(thisLambda);
      },
      [](auto&& thisLambda, const LayerNode* layerNode) {
        layerNode->visitChildren(thisLambda);
      },
      [](auto&& thisLambda, const GroupNode* groupNode) {
        groupNode->visitChildren(thisLambda);
      },
      [](EntityNode* entityNode) {
        auto entity = entityNode->entity();
        entity.resetLinkId();
        entityNode->setEntity(std::move(entity));
      },
      [](const BrushNode*) {},
      [](const PatchNode*) {}));
}

GroupNode::GroupNode(Group group)
  : m_group{std::move(group)}
{
}

const Group& GroupNode::group() const
{
  return m_group;
}

Group GroupNode::setGroup(Group group)
{
  using std::swap;
  swap(m_group, group);
  return group;
}

bool GroupNode::opened() const
{
  return m_editState == EditState::Open;
}

bool GroupNode::hasOpenedDescendant() const
{
  return m_editState == EditState::DescendantOpen;
}

bool GroupNode::closed() const
{
  return m_editState == EditState::Closed;
}

void GroupNode::open()
{
  assert(m_editState == EditState::Closed);
  setEditState(EditState::Open);
  openAncestors();
}

void GroupNode::close()
{
  assert(m_editState == EditState::Open);
  setEditState(EditState::Closed);
  closeAncestors();
}

const std::optional<IdType>& GroupNode::persistentId() const
{
  return m_persistentId;
}

void GroupNode::setPersistentId(const IdType persistentId)
{
  m_persistentId = persistentId;
}

void GroupNode::resetPersistentId()
{
  m_persistentId = std::nullopt;
}

bool GroupNode::hasPendingChanges() const
{
  return m_hasPendingChanges;
}

void GroupNode::setHasPendingChanges(const bool hasPendingChanges)
{
  m_hasPendingChanges = hasPendingChanges;
}

void GroupNode::setEditState(const EditState editState)
{
  m_editState = editState;
}

void GroupNode::setAncestorEditState(const EditState editState)
{
  visitParent(kdl::overload(
    [=](auto&& thisLambda, WorldNode* world) -> void { world->visitParent(thisLambda); },
    [=](auto&& thisLambda, LayerNode* layer) -> void { layer->visitParent(thisLambda); },
    [=](auto&& thisLambda, GroupNode* group) -> void {
      group->setEditState(editState);
      group->visitParent(thisLambda);
    },
    [=](
      auto&& thisLambda, EntityNode* entity) -> void { entity->visitParent(thisLambda); },
    [=](auto&& thisLambda, BrushNode* brush) -> void { brush->visitParent(thisLambda); },
    [=](
      auto&& thisLambda, PatchNode* patch) -> void { patch->visitParent(thisLambda); }));
}

void GroupNode::openAncestors()
{
  setAncestorEditState(EditState::DescendantOpen);
}

void GroupNode::closeAncestors()
{
  setAncestorEditState(EditState::Closed);
}

const std::string& GroupNode::doGetName() const
{
  return m_group.name();
}

const vm::bbox3& GroupNode::doGetLogicalBounds() const
{
  if (!m_boundsValid)
  {
    validateBounds();
  }
  return m_logicalBounds;
}

const vm::bbox3& GroupNode::doGetPhysicalBounds() const
{
  if (!m_boundsValid)
  {
    validateBounds();
  }
  return m_physicalBounds;
}

FloatType GroupNode::doGetProjectedArea(const vm::axis::type) const
{
  return static_cast<FloatType>(0);
}

Node* GroupNode::doClone(const vm::bbox3& /* worldBounds */) const
{
  auto groupNode = std::make_unique<GroupNode>(m_group);
  cloneAttributes(groupNode.get());
  return groupNode.release();
}

/** Check whether the given parent node or any of its ancestors and the given group node
 *  or any of its descendants have the same linked group id.
 */
static bool checkRecursiveLinkedGroups(
  const Node& parentNode, const GroupNode& groupNodeToAdd)
{
  const auto ancestorLinkedGroupIds = [&]() {
    auto result = std::vector<std::string>{};
    const auto* node = &parentNode;
    while (node)
    {
      const auto* groupNode = dynamic_cast<const Model::GroupNode*>(node);
      const auto linkedGroupId =
        groupNode ? groupNode->group().linkedGroupId() : std::nullopt;
      if (linkedGroupId)
      {
        result.push_back(*linkedGroupId);
      }
      node = node->parent();
    }
    return kdl::vec_sort_and_remove_duplicates(std::move(result));
  }();

  const auto linkedGroupIdsToAdd = [&]() {
    auto result = std::vector<std::string>{};
    groupNodeToAdd.accept(kdl::overload(
      [](const WorldNode*) {},
      [](const LayerNode*) {},
      [&](auto&& thisLambda, const GroupNode* groupNode) {
        if (const auto linkedGroupId = groupNode->group().linkedGroupId())
        {
          result.push_back(*linkedGroupId);
        }
        Node::visitAll(groupNode->children(), thisLambda);
      },
      [](const EntityNode*) {},
      [](const BrushNode*) {},
      [](const PatchNode*) {}));
    return kdl::vec_sort_and_remove_duplicates(std::move(result));
  }();

  return kdl::set_has_shared_element(ancestorLinkedGroupIds, linkedGroupIdsToAdd);
}

bool GroupNode::doCanAddChild(const Node* child) const
{
  return child->accept(kdl::overload(
    [](const WorldNode*) { return false; },
    [](const LayerNode*) { return false; },
    [&](const GroupNode* groupNode) {
      return !checkRecursiveLinkedGroups(*this, *groupNode);
    },
    [](const EntityNode*) { return true; },
    [](const BrushNode*) { return true; },
    [](const PatchNode*) { return true; }));
}

bool GroupNode::doCanRemoveChild(const Node* /* child */) const
{
  return true;
}

bool GroupNode::doRemoveIfEmpty() const
{
  return true;
}

bool GroupNode::doShouldAddToSpacialIndex() const
{
  return false;
}

void GroupNode::doChildWasAdded(Node* /* node */)
{
  nodePhysicalBoundsDidChange();
}

void GroupNode::doChildWasRemoved(Node* /* node */)
{
  nodePhysicalBoundsDidChange();
}

void GroupNode::doNodePhysicalBoundsDidChange()
{
  invalidateBounds();
}

void GroupNode::doChildPhysicalBoundsDidChange()
{
  invalidateBounds();
  nodePhysicalBoundsDidChange();
}

bool GroupNode::doSelectable() const
{
  return true;
}

void GroupNode::doPick(const EditorContext&, const vm::ray3& /* ray */, PickResult&)
{
  // For composite nodes (Groups, brush entities), pick rays don't hit the group
  // but instead just the primitives inside (brushes, point entities).
  // This avoids a potential performance trap where we'd have to exhaustively
  // test many objects if most of the map was inside groups, but it means
  // the pick results need to be postprocessed to account for groups (if desired).
  // See: https://github.com/TrenchBroom/TrenchBroom/issues/2742
}

void GroupNode::doFindNodesContaining(const vm::vec3& point, std::vector<Node*>& result)
{
  if (logicalBounds().contains(point))
  {
    result.push_back(this);
  }

  for (auto* child : Node::children())
  {
    child->findNodesContaining(point, result);
  }
}

void GroupNode::doAccept(NodeVisitor& visitor)
{
  visitor.visit(this);
}

void GroupNode::doAccept(ConstNodeVisitor& visitor) const
{
  visitor.visit(this);
}

Node* GroupNode::doGetContainer()
{
  return parent();
}

LayerNode* GroupNode::doGetContainingLayer()
{
  return findContainingLayer(this);
}

GroupNode* GroupNode::doGetContainingGroup()
{
  return findContainingGroup(this);
}

void GroupNode::invalidateBounds()
{
  m_boundsValid = false;
}

void GroupNode::validateBounds() const
{
  m_logicalBounds = computeLogicalBounds(children(), vm::bbox3{0.0});
  m_physicalBounds = computePhysicalBounds(children(), vm::bbox3{0.0});
  m_boundsValid = true;
}

void GroupNode::doAcceptTagVisitor(TagVisitor& visitor)
{
  visitor.visit(*this);
}

void GroupNode::doAcceptTagVisitor(ConstTagVisitor& visitor) const
{
  visitor.visit(*this);
}
} // namespace TrenchBroom::Model
