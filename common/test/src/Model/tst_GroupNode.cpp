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

#include "Error.h"
#include "Model/BezierPatch.h"
#include "Model/Brush.h"
#include "Model/BrushBuilder.h"
#include "Model/BrushNode.h"
#include "Model/Entity.h"
#include "Model/EntityNode.h"
#include "Model/Group.h"
#include "Model/GroupNode.h"
#include "Model/Layer.h"
#include "Model/LayerNode.h"
#include "Model/PatchNode.h"
#include "Model/WorldNode.h"
#include "TestUtils.h"

#include <kdl/result.h>

#include <vecmath/bbox.h>
#include <vecmath/bbox_io.h>
#include <vecmath/mat.h>
#include <vecmath/mat_ext.h>
#include <vecmath/mat_io.h>

#include <memory>
#include <vector>

#include "Catch2.h"

namespace TrenchBroom::Model
{

TEST_CASE("GroupNode.openAndClose")
{
  auto grandParentGroupNode = GroupNode{Group{"grandparent"}};
  auto* parentGroupNode = new GroupNode{Group{"parent"}};
  auto* groupNode = new GroupNode{Group{"group"}};
  auto* childGroupNode = new GroupNode{Group{"child"}};

  grandParentGroupNode.addChild(parentGroupNode);
  parentGroupNode->addChild(groupNode);
  groupNode->addChild(childGroupNode);

  REQUIRE_FALSE(grandParentGroupNode.opened());
  REQUIRE(grandParentGroupNode.closed());
  REQUIRE_FALSE(parentGroupNode->opened());
  REQUIRE(parentGroupNode->closed());
  REQUIRE_FALSE(groupNode->opened());
  REQUIRE(groupNode->closed());
  REQUIRE_FALSE(childGroupNode->opened());
  REQUIRE(childGroupNode->closed());

  REQUIRE_FALSE(grandParentGroupNode.hasOpenedDescendant());
  REQUIRE_FALSE(parentGroupNode->hasOpenedDescendant());
  REQUIRE_FALSE(groupNode->hasOpenedDescendant());
  REQUIRE_FALSE(childGroupNode->hasOpenedDescendant());

  groupNode->open();
  CHECK_FALSE(grandParentGroupNode.opened());
  CHECK_FALSE(grandParentGroupNode.closed());
  CHECK_FALSE(parentGroupNode->opened());
  CHECK_FALSE(parentGroupNode->closed());
  CHECK(groupNode->opened());
  CHECK_FALSE(groupNode->closed());
  CHECK_FALSE(childGroupNode->opened());
  CHECK(childGroupNode->closed());

  CHECK(grandParentGroupNode.hasOpenedDescendant());
  CHECK(parentGroupNode->hasOpenedDescendant());
  CHECK_FALSE(groupNode->hasOpenedDescendant());
  CHECK_FALSE(childGroupNode->hasOpenedDescendant());

  groupNode->close();
  CHECK_FALSE(grandParentGroupNode.opened());
  CHECK(grandParentGroupNode.closed());
  CHECK_FALSE(parentGroupNode->opened());
  CHECK(parentGroupNode->closed());
  CHECK_FALSE(groupNode->opened());
  CHECK(groupNode->closed());
  CHECK_FALSE(childGroupNode->opened());
  CHECK(childGroupNode->closed());

  CHECK_FALSE(grandParentGroupNode.hasOpenedDescendant());
  CHECK_FALSE(parentGroupNode->hasOpenedDescendant());
  CHECK_FALSE(groupNode->hasOpenedDescendant());
  CHECK_FALSE(childGroupNode->hasOpenedDescendant());
}

TEST_CASE("GroupNode.canAddChild")
{
  constexpr auto worldBounds = vm::bbox3d{8192.0};
  constexpr auto mapFormat = MapFormat::Quake3;

  auto worldNode = WorldNode{{}, {}, mapFormat};
  auto layerNode = LayerNode{Layer{"layer"}};
  auto groupNode = GroupNode{Group{"group"}};
  auto entityNode = EntityNode{Entity{}};
  auto brushNode =
    BrushNode{BrushBuilder{mapFormat, worldBounds}.createCube(64.0, "texture").value()};

  // clang-format off
  auto patchNode = PatchNode{BezierPatch{3, 3, {
    {0, 0, 0}, {1, 0, 1}, {2, 0, 0},
    {0, 1, 1}, {1, 1, 2}, {2, 1, 1},
    {0, 2, 0}, {1, 2, 1}, {2, 2, 0} }, "texture"}};
  // clang-format on

  CHECK_FALSE(groupNode.canAddChild(&worldNode));
  CHECK_FALSE(groupNode.canAddChild(&layerNode));
  CHECK_FALSE(groupNode.canAddChild(&groupNode));
  CHECK(groupNode.canAddChild(&entityNode));
  CHECK(groupNode.canAddChild(&brushNode));
  CHECK(groupNode.canAddChild(&patchNode));

  SECTION("Recursive linked groups")
  {
    auto linkedGroupNode = std::make_unique<GroupNode>(Group{"group"});
    setLinkedGroupId(groupNode, "linked_group_id");
    setLinkedGroupId(*linkedGroupNode, *groupNode.group().linkedGroupId());
    CHECK_FALSE(groupNode.canAddChild(linkedGroupNode.get()));

    auto outerGroupNode = GroupNode{Group{"outer_group"}};
    outerGroupNode.addChild(linkedGroupNode.release());
    CHECK_FALSE(groupNode.canAddChild(&outerGroupNode));
  }
}

TEST_CASE("GroupNode.canRemoveChild")
{
  constexpr auto worldBounds = vm::bbox3d{8192.0};
  constexpr auto mapFormat = MapFormat::Quake3;

  const auto worldNode = WorldNode{{}, {}, mapFormat};
  auto layerNode = LayerNode{Layer{"layer"}};
  auto groupNode = GroupNode{Group{"group"}};
  auto entityNode = EntityNode{Entity{}};
  auto brushNode =
    BrushNode{BrushBuilder{mapFormat, worldBounds}.createCube(64.0, "texture").value()};

  // clang-format off
  auto patchNode = PatchNode{BezierPatch{3, 3, {
    {0, 0, 0}, {1, 0, 1}, {2, 0, 0},
    {0, 1, 1}, {1, 1, 2}, {2, 1, 1},
    {0, 2, 0}, {1, 2, 1}, {2, 2, 0} }, "texture"}};
  // clang-format on

  CHECK(groupNode.canRemoveChild(&worldNode));
  CHECK(groupNode.canRemoveChild(&layerNode));
  CHECK(groupNode.canRemoveChild(&groupNode));
  CHECK(groupNode.canRemoveChild(&entityNode));
  CHECK(groupNode.canRemoveChild(&brushNode));
  CHECK(groupNode.canRemoveChild(&patchNode));
}

TEST_CASE("GroupNode.updateLinkedGroups")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto groupNode = GroupNode{Group{"name"}};
  auto* entityNode = new EntityNode{Entity{}};
  groupNode.addChild(entityNode);

  transformNode(groupNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
  REQUIRE(
    groupNode.group().transformation()
    == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
  REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));

  SECTION("Target group list is empty")
  {
    updateLinkedGroups(groupNode, {}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) { CHECK(r.empty()); })
      .transform_error([](const auto&) { FAIL(); });
  }

  SECTION("Target group list contains only source group")
  {
    updateLinkedGroups(groupNode, {&groupNode}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) { CHECK(r.empty()); })
      .transform_error([](const auto&) { FAIL(); });
  }

  SECTION("Update a single target group")
  {
    auto groupNodeClone = std::unique_ptr<GroupNode>{
      static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
    REQUIRE(
      groupNodeClone->group().transformation()
      == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));

    transformNode(
      *groupNodeClone, vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)), worldBounds);
    REQUIRE(
      groupNodeClone->group().transformation()
      == vm::translation_matrix(vm::vec3(1.0, 2.0, 0.0)));
    REQUIRE(
      static_cast<EntityNode*>(groupNodeClone->children().front())->entity().origin()
      == vm::vec3(1.0, 2.0, 0.0));

    transformNode(
      *entityNode, vm::translation_matrix(vm::vec3(0.0, 0.0, 3.0)), worldBounds);
    REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 3.0));

    updateLinkedGroups(groupNode, {groupNodeClone.get()}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) {
        CHECK(r.size() == 1u);

        const auto& p = r.front();
        const auto& [groupNodeToUpdate, newChildren] = p;

        CHECK(groupNodeToUpdate == groupNodeClone.get());
        CHECK(newChildren.size() == 1u);

        const auto* newEntityNode = dynamic_cast<EntityNode*>(newChildren.front().get());
        CHECK(newEntityNode != nullptr);

        CHECK(newEntityNode->entity().origin() == vm::vec3(1.0, 2.0, 3.0));
      })
      .transform_error([](const auto&) { FAIL(); });
  }
}

TEST_CASE("GroupNode.updateNestedLinkedGroups")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto outerGroupNode = GroupNode{Group{"outer"}};
  auto* innerGroupNode = new GroupNode{Group{"inner"}};
  outerGroupNode.addChild(innerGroupNode);

  auto* innerGroupEntityNode = new EntityNode{Entity{}};
  innerGroupNode->addChild(innerGroupEntityNode);

  auto innerGroupNodeClone = std::unique_ptr<GroupNode>{
    static_cast<GroupNode*>(innerGroupNode->cloneRecursively(worldBounds))};
  REQUIRE(innerGroupNodeClone->group().transformation() == vm::mat4x4());

  transformNode(
    *innerGroupNodeClone, vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)), worldBounds);
  REQUIRE(
    innerGroupNodeClone->group().transformation()
    == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

  SECTION("Transforming the inner group node and updating the linked group")
  {
    transformNode(
      *innerGroupNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
    REQUIRE(outerGroupNode.group().transformation() == vm::mat4x4());
    REQUIRE(
      innerGroupNode->group().transformation()
      == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
    REQUIRE(innerGroupEntityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));
    REQUIRE(
      innerGroupNodeClone->group().transformation()
      == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

    updateLinkedGroups(*innerGroupNode, {innerGroupNodeClone.get()}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) {
        CHECK(r.size() == 1u);

        const auto& p = r.front();
        const auto& [groupNodeToUpdate, newChildren] = p;

        CHECK(groupNodeToUpdate == innerGroupNodeClone.get());
        CHECK(newChildren.size() == 1u);

        const auto* newEntityNode = dynamic_cast<EntityNode*>(newChildren.front().get());
        CHECK(newEntityNode != nullptr);

        CHECK(newEntityNode->entity().origin() == vm::vec3(0.0, 2.0, 0.0));
      })
      .transform_error([](const auto&) { FAIL(); });
  }

  SECTION("Transforming the inner group node's entity and updating the linked group")
  {
    transformNode(
      *innerGroupEntityNode,
      vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)),
      worldBounds);
    REQUIRE(outerGroupNode.group().transformation() == vm::mat4x4());
    REQUIRE(innerGroupNode->group().transformation() == vm::mat4x4());
    REQUIRE(innerGroupEntityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));
    REQUIRE(
      innerGroupNodeClone->group().transformation()
      == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

    updateLinkedGroups(*innerGroupNode, {innerGroupNodeClone.get()}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) {
        CHECK(r.size() == 1u);

        const auto& p = r.front();
        const auto& [groupNodeToUpdate, newChildren] = p;

        CHECK(groupNodeToUpdate == innerGroupNodeClone.get());
        CHECK(newChildren.size() == 1u);

        const auto* newEntityNode = dynamic_cast<EntityNode*>(newChildren.front().get());
        CHECK(newEntityNode != nullptr);

        CHECK(newEntityNode->entity().origin() == vm::vec3(1.0, 2.0, 0.0));
      })
      .transform_error([](const auto&) { FAIL(); });
  }
}

TEST_CASE("GroupNode.updateLinkedGroupsRecursively")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto outerGroupNode = GroupNode{Group{"outer"}};

  /*
  outerGroupNode
  */

  auto* innerGroupNode = new GroupNode{Group{"inner"}};
  outerGroupNode.addChild(innerGroupNode);

  /*
  outerGroupNode
  +- innerGroupNode
  */

  auto* innerGroupEntityNode = new EntityNode{Entity{}};
  innerGroupNode->addChild(innerGroupEntityNode);

  /*
  outerGroupNode
  +-innerGroupNode
     +-innerGroupEntityNode
  */

  auto outerGroupNodeClone = std::unique_ptr<GroupNode>{
    static_cast<GroupNode*>(outerGroupNode.cloneRecursively(worldBounds))};
  REQUIRE(outerGroupNodeClone->group().transformation() == vm::mat4x4());
  REQUIRE(outerGroupNodeClone->childCount() == 1u);

  /*
  outerGroupNode
  +-innerGroupNode
     +-innerGroupEntityNode
  outerGroupNodeClone
  +-innerGroupNodeClone
     +-innerGroupEntityNodeClone
  */

  auto* innerGroupNodeClone =
    dynamic_cast<GroupNode*>(outerGroupNodeClone->children().front());
  REQUIRE(innerGroupNodeClone != nullptr);
  REQUIRE(innerGroupNodeClone->childCount() == 1u);

  auto* innerGroupEntityNodeClone =
    dynamic_cast<EntityNode*>(innerGroupNodeClone->children().front());
  REQUIRE(innerGroupEntityNodeClone != nullptr);

  updateLinkedGroups(outerGroupNode, {outerGroupNodeClone.get()}, worldBounds)
    .transform([&](const UpdateLinkedGroupsResult& r) {
      REQUIRE(r.size() == 1u);
      const auto& [groupNodeToUpdate, newChildren] = r.front();

      REQUIRE(groupNodeToUpdate == outerGroupNodeClone.get());
      REQUIRE(newChildren.size() == 1u);

      auto* newInnerGroupNodeClone = dynamic_cast<GroupNode*>(newChildren.front().get());
      CHECK(newInnerGroupNodeClone != nullptr);
      CHECK(newInnerGroupNodeClone->group() == innerGroupNode->group());
      CHECK(newInnerGroupNodeClone->childCount() == 1u);

      auto* newInnerGroupEntityNodeClone =
        dynamic_cast<EntityNode*>(newInnerGroupNodeClone->children().front());
      CHECK(newInnerGroupEntityNodeClone != nullptr);
      CHECK(newInnerGroupEntityNodeClone->entity() == innerGroupEntityNode->entity());
    })
    .transform_error([](const auto&) { FAIL(); });
}

TEST_CASE("GroupNode.updateLinkedGroupsExceedsWorldBounds")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto groupNode = GroupNode{Group{"name"}};
  auto* entityNode = new EntityNode{Entity{}};
  groupNode.addChild(entityNode);

  auto groupNodeClone = std::unique_ptr<GroupNode>{
    static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};

  transformNode(
    *groupNodeClone,
    vm::translation_matrix(vm::vec3(8192.0 - 8.0, 0.0, 0.0)),
    worldBounds);
  REQUIRE(
    groupNodeClone->children().front()->logicalBounds()
    == vm::bbox3(vm::vec3(8192.0 - 16.0, -8.0, -8.0), vm::vec3(8192.0, 8.0, 8.0)));

  transformNode(
    *entityNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
  REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));

  updateLinkedGroups(groupNode, {groupNodeClone.get()}, worldBounds)
    .transform([](auto) { FAIL(); })
    .transform_error([](auto e) {
      CHECK(e == Error{"Updating a linked node would exceed world bounds"});
    });
}

static void setGroupName(GroupNode& groupNode, const std::string& name)
{
  auto group = groupNode.group();
  group.setName(name);
  groupNode.setGroup(std::move(group));
}

TEST_CASE("GroupNode.updateLinkedGroupsAndPreserveNestedGroupNames")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto outerGroupNode = GroupNode{Group{"outerGroupNode"}};
  auto* innerGroupNode = new GroupNode{Group{"innerGroupNode"}};
  outerGroupNode.addChild(innerGroupNode);

  auto innerGroupNodeClone = std::unique_ptr<GroupNode>(
    static_cast<GroupNode*>(innerGroupNode->cloneRecursively(worldBounds)));
  setGroupName(*innerGroupNodeClone, "innerGroupNodeClone");

  auto outerGroupNodeClone = std::unique_ptr<GroupNode>(
    static_cast<GroupNode*>(outerGroupNode.cloneRecursively(worldBounds)));
  setGroupName(*outerGroupNodeClone, "outerGroupNodeClone");

  auto* innerGroupNodeNestedClone =
    static_cast<GroupNode*>(outerGroupNodeClone->children().front());
  setGroupName(*innerGroupNodeNestedClone, "innerGroupNodeNestedClone");

  /*
  outerGroupNode-------+
  +-innerGroupNode-----|-------+
  innerGroupNodeClone--|-------+
  outerGroupNodeClone--+       |
  +-innerGroupNodeNestedClone--+
   */

  SECTION(
    "Updating outerGroupNode retains the names of its linked group and the nested linked "
    "group")
  {
    updateLinkedGroups(outerGroupNode, {outerGroupNodeClone.get()}, worldBounds)
      .transform([&](const UpdateLinkedGroupsResult& r) {
        REQUIRE(r.size() == 1u);

        const auto& [groupNodeToUpdate, newChildren] = r.front();
        REQUIRE(groupNodeToUpdate == outerGroupNodeClone.get());

        const auto* innerReplacement = static_cast<GroupNode*>(newChildren.front().get());
        CHECK(innerReplacement->name() == innerGroupNodeNestedClone->name());
      })
      .transform_error([](const auto&) { FAIL(); });
  }
}

TEST_CASE("GroupNode.updateLinkedGroupsAndPreserveEntityProperties")
{
  const auto worldBounds = vm::bbox3(8192.0);

  auto sourceGroupNode = GroupNode{Group{"name"}};
  auto* sourceEntityNode = new EntityNode{Entity{}};
  sourceGroupNode.addChild(sourceEntityNode);

  auto targetGroupNode = std::unique_ptr<GroupNode>{
    static_cast<GroupNode*>(sourceGroupNode.cloneRecursively(worldBounds))};

  auto* targetEntityNode = static_cast<EntityNode*>(targetGroupNode->children().front());
  REQUIRE_THAT(
    targetEntityNode->entity().properties(),
    Catch::Equals(sourceEntityNode->entity().properties()));

  using T = std::tuple<
    std::vector<std::string>,
    std::vector<std::string>,
    std::vector<EntityProperty>,
    std::vector<EntityProperty>,
    std::vector<EntityProperty>>;

  // clang-format off
  const auto
  [srcProtProperties, trgtProtProperties, sourceProperties, 
                                          targetProperties, 
                                          expectedProperties ] = GENERATE(values<T>({
  // properties remain unchanged
  {{},                {},                 { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{},                { "some_key" },     { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{ "some_key" },    {},                 { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{ "some_key" },    { "some_key" },     { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  // property was added to source
  {{},                {},                 { { "some_key", "some_value" } },
                                          {},
                                          { { "some_key", "some_value" } } },

  {{},                { "some_key" },     { { "some_key", "some_value" } },
                                          {},
                                          {} },

  {{ "some_key" },    {},                 { { "some_key", "some_value" } },
                                          {},
                                          {} },

  {{ "some_key" },    { "some_key" },     { { "some_key", "some_value" } },
                                          {},
                                          {} },

  // property was changed in source
  {{},                {},                 { { "some_key", "other_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "other_value" } } },

  {{ "some_key" },    {},                 { { "some_key", "other_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{},                { "some_key" },     { { "some_key", "other_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{ "some_key" },    { "some_key" },     { { "some_key", "other_value" } },
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  // property was removed in source
  {{},                {},                 {},
                                          { { "some_key", "some_value" } },
                                          {} },

  {{ "some_key" },    {},                 {},
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{},                { "some_key" },     {},
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },

  {{ "some_key" },    { "some_key" },     {},
                                          { { "some_key", "some_value" } },
                                          { { "some_key", "some_value" } } },
  }));
  // clang-format on

  CAPTURE(
    srcProtProperties,
    trgtProtProperties,
    sourceProperties,
    targetProperties,
    expectedProperties);

  {
    auto entity = sourceEntityNode->entity();
    entity.setProperties({}, sourceProperties);
    entity.setProtectedProperties(srcProtProperties);
    sourceEntityNode->setEntity(std::move(entity));
  }

  {
    auto entity = targetEntityNode->entity();
    entity.setProperties({}, targetProperties);
    entity.setProtectedProperties(trgtProtProperties);
    targetEntityNode->setEntity(std::move(entity));
  }

  // lambda can't capture structured bindings
  const auto expectedTargetProperties = expectedProperties;

  updateLinkedGroups(sourceGroupNode, {targetGroupNode.get()}, worldBounds)
    .transform([&](const UpdateLinkedGroupsResult& r) {
      REQUIRE(r.size() == 1u);
      const auto& p = r.front();

      const auto& newChildren = p.second;
      REQUIRE(newChildren.size() == 1u);

      const auto* newEntityNode = dynamic_cast<EntityNode*>(newChildren.front().get());
      REQUIRE(newEntityNode != nullptr);

      CHECK_THAT(
        newEntityNode->entity().properties(),
        Catch::UnorderedEquals(expectedTargetProperties));
      CHECK_THAT(
        newEntityNode->entity().protectedProperties(),
        Catch::UnorderedEquals(targetEntityNode->entity().protectedProperties()));
    })
    .transform_error([](const auto&) { FAIL(); });
}

namespace
{
bool hasAnyEntityLinks(const Node& node)
{
  auto result = false;
  node.accept(kdl::overload(
    [](auto&& thisLambda, const WorldNode* worldNode) {
      worldNode->visitChildren(thisLambda);
    },
    [](auto&& thisLambda, const LayerNode* layerNode) {
      layerNode->visitChildren(thisLambda);
    },
    [](auto&& thisLambda, const GroupNode* groupNode) {
      groupNode->visitChildren(thisLambda);
    },
    [&](const EntityNode* entityNode) {
      result = result || entityNode->entity().linkId() != std::nullopt;
    },
    [](const BrushNode*) {},
    [](const PatchNode*) {}));
  return result;
}
} // namespace

TEST_CASE("GroupNode.setLinkIds")
{
  auto brushBuilder = Model::BrushBuilder{Model::MapFormat::Quake3, vm::bbox3{8192.0}};

  auto outerGroupNode = Model::GroupNode{Model::Group{"outer"}};
  auto* outerEntityNode = new Model::EntityNode{Model::Entity{}};
  auto* outerBrushNode =
    new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};

  auto* innerGroupNode = new Model::GroupNode{Model::Group{"inner"}};
  auto* innerBrushNode =
    new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};
  auto* innerEntityNode = new Model::EntityNode{Model::Entity{}};

  innerGroupNode->addChildren({innerBrushNode, innerEntityNode});
  outerGroupNode.addChildren({outerEntityNode, outerBrushNode, innerGroupNode});

  auto linkedOuterGroupNode = Model::GroupNode{Model::Group{"outer"}};
  auto* linkedOuterEntityNode = new Model::EntityNode{Model::Entity{}};
  auto* linkedOuterBrushNode =
    new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};

  auto* linkedInnerGroupNode = new Model::GroupNode{Model::Group{"inner"}};
  auto* linkedInnerBrushNode =
    new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};
  auto* linkedInnerEntityNode = new Model::EntityNode{Model::Entity{}};

  Model::setLinkedGroupId(outerGroupNode, "linkedGroupId");
  Model::setLinkedGroupId(linkedOuterGroupNode, "linkedGroupId");

  SECTION("If one outer group node has no children")
  {
    CHECK(
      setLinkIds({&outerGroupNode, &linkedOuterGroupNode})
      == Result<void>{Error{"Inconsistent linked group structure"}});
    CHECK(!hasAnyEntityLinks(outerGroupNode));
    CHECK(!hasAnyEntityLinks(linkedOuterGroupNode));
  }

  SECTION("If one outer group node has fewer children")
  {
    linkedOuterGroupNode.addChildren({linkedOuterEntityNode, linkedOuterBrushNode});
    CHECK(
      setLinkIds({&outerGroupNode, &linkedOuterGroupNode})
      == Result<void>{Error{"Inconsistent linked group structure"}});
    CHECK(!hasAnyEntityLinks(outerGroupNode));
    CHECK(!hasAnyEntityLinks(linkedOuterGroupNode));
  }

  SECTION("If one inner group node has fewer children")
  {
    linkedOuterGroupNode.addChildren(
      {linkedOuterEntityNode, linkedOuterBrushNode, linkedInnerGroupNode});
    linkedInnerGroupNode->addChildren({linkedInnerBrushNode});
    CHECK(
      setLinkIds({&outerGroupNode, &linkedOuterGroupNode})
      == Result<void>{Error{"Inconsistent linked group structure"}});
    CHECK(!hasAnyEntityLinks(outerGroupNode));
    CHECK(!hasAnyEntityLinks(linkedOuterGroupNode));
  }

  SECTION("If one outer group node has children in different order")
  {
    linkedInnerGroupNode->addChildren({linkedInnerBrushNode, linkedInnerEntityNode});
    linkedOuterGroupNode.addChildren(
      {linkedOuterEntityNode, linkedInnerGroupNode, linkedOuterBrushNode});
    CHECK(
      setLinkIds({&outerGroupNode, &linkedOuterGroupNode})
      == Result<void>{Error{"Inconsistent linked group structure"}});
    CHECK(!hasAnyEntityLinks(outerGroupNode));
    CHECK(!hasAnyEntityLinks(linkedOuterGroupNode));
  }

  SECTION("If one inner group node has children in different order")
  {
    linkedInnerGroupNode->addChildren({linkedInnerEntityNode, linkedInnerBrushNode});
    linkedOuterGroupNode.addChildren(
      {linkedOuterEntityNode, linkedOuterBrushNode, linkedInnerGroupNode});
    CHECK(
      setLinkIds({&outerGroupNode, &linkedOuterGroupNode})
      == Result<void>{Error{"Inconsistent linked group structure"}});
    CHECK(!hasAnyEntityLinks(outerGroupNode));
    CHECK(!hasAnyEntityLinks(linkedOuterGroupNode));
  }

  SECTION("If both groups have the same structure")
  {
    linkedInnerGroupNode->addChildren({linkedInnerBrushNode, linkedInnerEntityNode});
    linkedOuterGroupNode.addChildren(
      {linkedOuterEntityNode, linkedOuterBrushNode, linkedInnerGroupNode});

    SECTION("With less than two groups")
    {
      CHECK(
        setLinkIds({})
        == Result<void>{Error{"Link set must contain at least two groups"}});
      CHECK(
        setLinkIds({&outerGroupNode})
        == Result<void>{Error{"Link set must contain at least two groups"}});
    }

    SECTION("With two groups")
    {
      REQUIRE(outerEntityNode->entity().linkId() == std::nullopt);
      REQUIRE(innerEntityNode->entity().linkId() == std::nullopt);
      REQUIRE(outerEntityNode->entity() == linkedOuterEntityNode->entity());
      REQUIRE(innerEntityNode->entity() == linkedInnerEntityNode->entity());

      CHECK(setLinkIds({&outerGroupNode, &linkedOuterGroupNode}).is_success());

      CHECK(outerEntityNode->entity().linkId() != std::nullopt);
      CHECK(innerEntityNode->entity().linkId() != std::nullopt);
      CHECK(outerEntityNode->entity().linkId() != innerEntityNode->entity().linkId());
      CHECK(outerEntityNode->entity() == linkedOuterEntityNode->entity());
      CHECK(innerEntityNode->entity() == linkedInnerEntityNode->entity());
    }

    SECTION("With three groups")
    {
      auto linkedOuterGroupNode2 = Model::GroupNode{Model::Group{"outer"}};
      auto* linkedOuterEntityNode2 = new Model::EntityNode{Model::Entity{}};
      auto* linkedOuterBrushNode2 =
        new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};

      auto* linkedInnerGroupNode2 = new Model::GroupNode{Model::Group{"inner"}};
      auto* linkedInnerBrushNode2 =
        new Model::BrushNode{brushBuilder.createCube(64.0, "texture").value()};
      auto* linkedInnerEntityNode2 = new Model::EntityNode{Model::Entity{}};

      linkedInnerGroupNode2->addChildren({linkedInnerBrushNode2, linkedInnerEntityNode2});
      linkedOuterGroupNode2.addChildren(
        {linkedOuterEntityNode2, linkedOuterBrushNode2, linkedInnerGroupNode2});

      Model::setLinkedGroupId(linkedOuterGroupNode2, "linkedGroupId");

      REQUIRE(outerEntityNode->entity().linkId() == std::nullopt);
      REQUIRE(innerEntityNode->entity().linkId() == std::nullopt);
      REQUIRE(outerEntityNode->entity() == linkedOuterEntityNode->entity());
      REQUIRE(innerEntityNode->entity() == linkedInnerEntityNode->entity());
      REQUIRE(outerEntityNode->entity() == linkedOuterEntityNode2->entity());
      REQUIRE(innerEntityNode->entity() == linkedInnerEntityNode2->entity());

      CHECK(setLinkIds({&outerGroupNode, &linkedOuterGroupNode, &linkedOuterGroupNode2})
              .is_success());

      CHECK(outerEntityNode->entity().linkId() != std::nullopt);
      CHECK(innerEntityNode->entity().linkId() != std::nullopt);
      CHECK(outerEntityNode->entity().linkId() != innerEntityNode->entity().linkId());
      CHECK(outerEntityNode->entity() == linkedOuterEntityNode->entity());
      CHECK(innerEntityNode->entity() == linkedInnerEntityNode->entity());
      CHECK(outerEntityNode->entity() == linkedOuterEntityNode2->entity());
      CHECK(innerEntityNode->entity() == linkedInnerEntityNode2->entity());
    }

    SECTION("With nested linked groups")
    {
      Model::setLinkedGroupId(*innerGroupNode, "nestedLinkedGroupId");
      Model::setLinkedGroupId(*linkedInnerGroupNode, "nestedLinkedGroupId");

      SECTION("Only outer groups")
      {
        REQUIRE(outerEntityNode->entity().linkId() == std::nullopt);
        REQUIRE(innerEntityNode->entity().linkId() == std::nullopt);
        REQUIRE(outerEntityNode->entity() == linkedOuterEntityNode->entity());
        REQUIRE(innerEntityNode->entity() == linkedInnerEntityNode->entity());

        CHECK(setLinkIds({&outerGroupNode, &linkedOuterGroupNode}).is_success());

        CHECK(outerEntityNode->entity().linkId() != std::nullopt);
        CHECK(innerEntityNode->entity().linkId() == std::nullopt);
        CHECK(outerEntityNode->entity() == linkedOuterEntityNode->entity());
        CHECK(innerEntityNode->entity() == linkedInnerEntityNode->entity());
      }

      SECTION("Inner groups, then outer groups")
      {
        REQUIRE(outerEntityNode->entity().linkId() == std::nullopt);
        REQUIRE(innerEntityNode->entity().linkId() == std::nullopt);
        REQUIRE(outerEntityNode->entity() == linkedOuterEntityNode->entity());
        REQUIRE(innerEntityNode->entity() == linkedInnerEntityNode->entity());

        CHECK(setLinkIds({innerGroupNode, linkedInnerGroupNode}).is_success());

        CHECK(outerEntityNode->entity().linkId() == std::nullopt);
        CHECK(innerEntityNode->entity().linkId() != std::nullopt);
        CHECK(outerEntityNode->entity() == linkedOuterEntityNode->entity());
        CHECK(innerEntityNode->entity() == linkedInnerEntityNode->entity());

        const auto innerEntityLinkId = innerEntityNode->entity().linkId();

        CHECK(setLinkIds({&outerGroupNode, &linkedOuterGroupNode}).is_success());

        CHECK(outerEntityNode->entity().linkId() != std::nullopt);
        CHECK(innerEntityNode->entity().linkId() == innerEntityLinkId);
        CHECK(outerEntityNode->entity().linkId() != innerEntityLinkId);
        CHECK(outerEntityNode->entity() == linkedOuterEntityNode->entity());
        CHECK(innerEntityNode->entity() == linkedInnerEntityNode->entity());
      }
    }
  }
}

} // namespace TrenchBroom::Model
