// Copyright 2016 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "catch.hpp"

#include <iostream>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/stdx.hpp>

#include <mongo_odm/model.hpp>
#include <mongo_odm/odm_collection.hpp>
#include <mongo_odm/query_builder.hpp>

using namespace mongocxx;

// An ODM class that does not rely on model
class Point {
   public:
    int x;
    int y;
    MONGO_ODM_MAKE_KEYS(Point, MONGO_ODM_NVP(x), MONGO_ODM_NVP(y));
};

// An ODM class that inherits from model
class Bar : public mongo_odm::model<Bar> {
   public:
    int64_t w;
    int x1;
    stdx::optional<int> x2;
    bool y;
    std::string z;
    Point p;
    MONGO_ODM_MAKE_KEYS_MODEL(Bar, MONGO_ODM_NVP(w), MONGO_ODM_NVP(x1), MONGO_ODM_NVP(x2),
                              MONGO_ODM_NVP(y), MONGO_ODM_NVP(z), MONGO_ODM_NVP(p))

    Bar(int64_t w, int x1, stdx::optional<int> x2, bool y, std::string z, Point p)
        : w(w), x1(x1), x2(x2), y(y), z(z), p(p) {
        _id = bsoncxx::oid{bsoncxx::oid::init_tag_t{}};
    }

    // default constructor
    Bar() = default;

    bsoncxx::oid getID() {
        return _id;
    }
};

class BarParent {
   public:
    Bar b;

    BarParent() {
    }

    // default constructor
    BarParent(Bar b) : b(b) {
    }
    MONGO_ODM_MAKE_KEYS(BarParent, MONGO_ODM_NVP(b));
};

TEST_CASE("Test member access.", "[mongo_odm::Nvp]") {
    SECTION("Member access using MONGO_ODM_KEY") {
        auto nvp = MONGO_ODM_KEY(Bar::x1);
        REQUIRE(nvp.get_name() == "x1");
        REQUIRE(nvp.t == &Bar::x1);
    }

    SECTION("Member access using MONGO_ODM_CHILD macro") {
        auto nvp = MONGO_ODM_CHILD(Bar, p);
        REQUIRE(nvp.get_name() == "p");
        REQUIRE(nvp.t == &Bar::p);
    }
}

TEST_CASE("Test nested member access.", "[mongo_odm::NvpChild]") {
    SECTION("Nested member access using operator->*") {
        auto nvp_child = MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x);
        REQUIRE(nvp_child.get_name() == "p.x");
        REQUIRE(nvp_child.t == &Point::x);
        REQUIRE(nvp_child.parent.get_name() == "p");
        REQUIRE(nvp_child.parent.t == &Bar::p);
    }

    SECTION("Multiple nested member access using operator->*") {
        auto nvp_child =
            MONGO_ODM_KEY(BarParent::b)->*MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x);
        REQUIRE(nvp_child.get_name() == "b.p.x");
        REQUIRE(nvp_child.t == &Point::x);
        REQUIRE(nvp_child.parent.get_name() == "b.p");
        REQUIRE(nvp_child.parent.t == &Bar::p);
        REQUIRE(nvp_child.parent.parent.get_name() == "b");
        REQUIRE(nvp_child.parent.parent.t == &BarParent::b);
    }

    SECTION("Nested member access using MONGO_ODM_CHILD macro") {
        auto nvp_child = MONGO_ODM_CHILD(Bar, p, x);
        REQUIRE(nvp_child.get_name() == "p.x");
        REQUIRE(nvp_child.t == &Point::x);
        REQUIRE(nvp_child.parent.get_name() == "p");
        REQUIRE(nvp_child.parent.t == &Bar::p);
    }

    SECTION("Multiple nested member access using MONGO_ODM_CHILD macro") {
        auto nvp_child = MONGO_ODM_CHILD(BarParent, b, p, x);
        REQUIRE(nvp_child.get_name() == "b.p.x");
        REQUIRE(nvp_child.t == &Point::x);
        REQUIRE(nvp_child.parent.get_name() == "b.p");
        REQUIRE(nvp_child.parent.t == &Bar::p);
        REQUIRE(nvp_child.parent.parent.get_name() == "b");
        REQUIRE(nvp_child.parent.parent.t == &BarParent::b);
    }
}

TEST_CASE("Query Builder", "[mongo_odm::query_builder]") {
    instance::current();
    client conn{uri{}};
    collection coll = conn["testdb"]["testcollection"];
    coll.delete_many({});

    Bar::setCollection(coll);
    Bar(444, 1, 2, false, "hello", {0, 0}).save();
    Bar(444, 1, 3, false, "hello", {0, 1}).save();
    Bar(555, 10, 2, true, "goodbye", {1, 0}).save();

    SECTION("Test == comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) == 1);
        REQUIRE(res);
        REQUIRE(res.value().x1 == 1);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) == 3);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 3);

        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) == "hello");
        REQUIRE(res);
        REQUIRE(res.value().z == "hello");

        // nested member test
        res = Bar::find_one(MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x) == 1);
        REQUIRE(res);
        REQUIRE(res.value().p.x == 1);
    }

    SECTION("Test > comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) > 1);
        REQUIRE(res);
        REQUIRE(res.value().x1 > 1);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) > 2);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 3);

        // Test lexicographical comparison
        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) > "goodbye");
        REQUIRE(res);
        REQUIRE(res.value().z == "hello");

        // nested member test
        res = Bar::find_one(MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x) > 0);
        REQUIRE(res);
        REQUIRE(res.value().p.x > 0);
    }

    SECTION("Test >= comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) >= 10);
        REQUIRE(res);
        REQUIRE(res.value().x1 >= 10);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) >= 3);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 3);

        // Test lexicographical comparison
        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) >= "hello");
        REQUIRE(res);
        REQUIRE(res.value().z == "hello");
    }

    SECTION("Test < comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) < 10);
        REQUIRE(res);
        REQUIRE(res.value().x1 < 10);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) < 3);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 2);

        // Test lexicographical comparison
        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) < "hello");
        REQUIRE(res);
        REQUIRE(res.value().z == "goodbye");
    }

    SECTION("Test <= comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) <= 1);
        REQUIRE(res);
        REQUIRE(res.value().x1 <= 1);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) <= 2);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 2);

        // Test lexicographical comparison
        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) <= "goodbye");
        REQUIRE(res);
        REQUIRE(res.value().z == "goodbye");
    }

    SECTION("Test != comparison.", "[mongo_odm::ComparisonExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) != 1);
        REQUIRE(res);
        REQUIRE(res.value().x1 != 1);

        // Test optional type
        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2) != 2);
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 3);

        res = Bar::find_one(MONGO_ODM_KEY(Bar::z) != "hello");
        REQUIRE(res);
        REQUIRE(res.value().z == "goodbye");

        // nested member test
        res = Bar::find_one(MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x) != 0);
        REQUIRE(res);
        REQUIRE(res.value().p.x != 0);
    }

    SECTION("Test $not expression, with operator!.", "[mongo_odm::NotExpr]") {
        auto res = Bar::find_one(!(MONGO_ODM_KEY(Bar::x1) < 10));
        REQUIRE(res);
        REQUIRE(res.value().x1 >= 10);

        res = Bar::find_one(!(MONGO_ODM_KEY(Bar::z) == "hello"));
        REQUIRE(res);
        REQUIRE(res.value().z == "goodbye");

        // Test optional type
        res = Bar::find_one(!(MONGO_ODM_KEY(Bar::x2) <= 2));
        REQUIRE(res);
        REQUIRE(res.value().x2.value() == 3);

        // nested member test
        res = Bar::find_one(!(MONGO_ODM_KEY(Bar::p)->*MONGO_ODM_KEY(Point::x) == 1));
        REQUIRE(res);
        REQUIRE(res.value().p.x != 1);
    }

    SECTION("Test $nin and $in operators.", "[mongo_odm::InArrayExpr]") {
        std::vector<int> nums;
        nums.push_back(1);
        nums.push_back(2);
        nums.push_back(555);

        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::w).in(nums));
        REQUIRE(res);
        REQUIRE(res.value().w == 555);

        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2).in(nums));
        REQUIRE(res);
        REQUIRE(res.value().x2 == 2);

        nums.clear();
        nums.push_back(1);
        nums.push_back(2);
        nums.push_back(444);

        res = Bar::find_one(MONGO_ODM_KEY(Bar::w).nin(nums));
        REQUIRE(res);
        REQUIRE(res.value().w == 555);

        res = Bar::find_one(MONGO_ODM_KEY(Bar::x2).nin(nums));
        REQUIRE(res);
        REQUIRE(res.value().x2 == 3);

        res = Bar::find_one(MONGO_ODM_CHILD(Bar, p, y).nin(nums) && MONGO_ODM_KEY(Bar::w) == 555);
        REQUIRE(res);
        REQUIRE(res.value().w == 555);
        REQUIRE(res.value().p.y == 0);
    }

    SECTION("Test expression list.", "[mongo_odm::ExpressionList]") {
        auto res = Bar::find_one((MONGO_ODM_KEY(Bar::x1) == 1, MONGO_ODM_KEY(Bar::x2) == 2,
                                  MONGO_ODM_KEY(Bar::w) >= 444, MONGO_ODM_CHILD(Bar, p, y) == 0));
        REQUIRE(res);
        REQUIRE(res.value().x1 == 1);
        REQUIRE(res.value().x2 == 2);
        REQUIRE(res.value().w >= 444);
        REQUIRE(res.value().p.x == 0);

        // Test variadic ExpressionList builder. ODM clients shouldn't need to use this,
        // but it's used internally by functions such as nor(...) that accept variadic arguments.
        res = Bar::find_one(mongo_odm::make_expression_list(
            MONGO_ODM_KEY(Bar::x1) == 1, MONGO_ODM_KEY(Bar::x2) == 2, MONGO_ODM_KEY(Bar::w) >= 444,
            MONGO_ODM_CHILD(Bar, p, y) == 0));
        REQUIRE(res);
        REQUIRE(res.value().x1 == 1);
        REQUIRE(res.value().x2 == 2);
        REQUIRE(res.value().w >= 444);
        REQUIRE(res.value().p.x == 0);
    }

    SECTION("Test boolean expressions.", "[mongo_odm::BooleanExpr]") {
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x1) > 9 && MONGO_ODM_KEY(Bar::x1) < 11 &&
                                 MONGO_ODM_CHILD(Bar, p, x) > 0);
        REQUIRE(res);
        REQUIRE(res.value().x1 == 10);
        REQUIRE(res.value().x1 == 10);

        auto cursor = Bar::find(MONGO_ODM_KEY(Bar::x1) == 10 || MONGO_ODM_KEY(Bar::x2) == 3 ||
                                MONGO_ODM_CHILD(Bar, p, x) == 1);
        int i = 0;
        for (Bar b : cursor) {
            i++;
            // Check that x1 is equal to 10 or x2 is equal to 3
            bool or_test = (b.x1 == 10) || (b.x2 == 3);
            REQUIRE(or_test);
        }
        REQUIRE(i == 2);

        // Test a complex boolean expression, with parentheses and mixed oeprators
        res = Bar::find_one(
            (MONGO_ODM_KEY(Bar::z) == "goodbye" || !(MONGO_ODM_KEY(Bar::y) == false)) &&
            (MONGO_ODM_KEY(Bar::w) == 555 || MONGO_ODM_KEY(Bar::x2) == 3));
        REQUIRE(res);
        REQUIRE(res.value().z == "goodbye");
    }

    SECTION("Test boolean operators on expression lists. (used primarily for $nor)",
            "[mongo_odm::BooleanListExpr]") {
        auto res =
            Bar::find_one(nor(MONGO_ODM_KEY(Bar::w) == 444, MONGO_ODM_KEY(Bar::x1) == 1,
                              MONGO_ODM_KEY(Bar::x2) == 3, MONGO_ODM_KEY(Bar::y) == false,
                              MONGO_ODM_KEY(Bar::z) == "hello", MONGO_ODM_CHILD(Bar, p, x) == 0));
        REQUIRE(res);
        Bar b = res.value();
        REQUIRE(b.w != 444);
        REQUIRE(b.x1 != 1);
        REQUIRE(b.x2 != 3);
        REQUIRE(b.y != false);
        REQUIRE(b.z != "hello");
        REQUIRE(b.p.x != 0);
    }

    SECTION("Test $exists operator on optional fields.", "[mongo_odm::Nvp::exists]") {
        // auto query = (bsoncxx::document::view_or_value)(MONGO_ODM_KEY(Bar::x2).exists());
        // std::cout << bsoncxx::to_json(query.view()) << std::endl;

        auto count = coll.count(MONGO_ODM_KEY(Bar::x2).exists(true));
        REQUIRE(count == 3);

        Bar(444, 1, stdx::nullopt, false, "hello", {0, 0}).save();
        auto res = Bar::find_one(MONGO_ODM_KEY(Bar::x2).exists(false));
        REQUIRE(res);
        REQUIRE(!res.value().x2);
    }
}

TEST_CASE("Query builder works with non-ODM class") {
    instance::current();
    client conn{uri{}};
    collection coll = conn["testdb"]["testcollection"];
    coll.delete_many({});

    auto point_coll = mongo_odm::odm_collection<Point>(coll);
    point_coll.insert_one({5, 6});
    auto res = point_coll.find_one(MONGO_ODM_KEY(Point::x) == 5);
    REQUIRE(res.value().x == 5);

    coll.delete_many({});
}

TEST_CASE("Update Builder", "mongo_odm::UpdateExpr") {
    instance::current();
    client conn{uri{}};
    collection coll = conn["testdb"]["testcollection"];
    coll.delete_many({});

    Bar::setCollection(coll);
    Bar(444, 1, 2, false, "hello", {0, 0}).save();
    Bar(444, 1, 3, false, "hello", {0, 1}).save();
    Bar(555, 10, 2, true, "goodbye", {1, 0}).save();

    SECTION("Test = assignment.", "[mongo_odm::UpdateExpr]") {
        auto res = coll.update_one(MONGO_ODM_KEY(Bar::w) == 555,
                                   (MONGO_ODM_KEY(Bar::x1) = 73, MONGO_ODM_KEY(Bar::x2) = 99,
                                    MONGO_ODM_CHILD(Bar, p, x) = 100));
        REQUIRE(res);
        REQUIRE(res.value().modified_count() == 1);
        auto bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
        REQUIRE(bar);
        REQUIRE(bar.value().x1 == 73);
        REQUIRE(bar.value().x2.value() == 99);
        REQUIRE(bar.value().p.x == 100);
    }

    SECTION("Test increment operators.", "[mongo_odm::UpdateExpr]") {
        auto bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
        REQUIRE(bar);
        int initial_x1 = bar.value().x1;
        int initial_x2 = bar.value().x2.value();
        int initial_p_x = bar.value().p.x;

        {
            // Test postfix ++
            auto res = coll.update_one(
                MONGO_ODM_KEY(Bar::w) == 555,
                (MONGO_ODM_KEY(Bar::x1)++, MONGO_ODM_KEY(Bar::x2)++, MONGO_ODM_CHILD(Bar, p, x)++));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 + 1);
            REQUIRE(bar.value().x2.value() == initial_x2 + 1);
            REQUIRE(bar.value().p.x == initial_p_x + 1);
        }

        {
            // Test prefix ++
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(
                MONGO_ODM_KEY(Bar::w) == 555,
                (++MONGO_ODM_KEY(Bar::x1), ++MONGO_ODM_KEY(Bar::x2), ++MONGO_ODM_CHILD(Bar, p, x)));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 + 1);
            REQUIRE(bar.value().x2.value() == initial_x2 + 1);
            REQUIRE(bar.value().p.x == initial_p_x + 1);
        }

        {
            // Test postfix --
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(
                MONGO_ODM_KEY(Bar::w) == 555,
                (MONGO_ODM_KEY(Bar::x1)--, MONGO_ODM_KEY(Bar::x2)--, MONGO_ODM_CHILD(Bar, p, x)--));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 - 1);
            REQUIRE(bar.value().x2.value() == initial_x2 - 1);
            REQUIRE(bar.value().p.x == initial_p_x - 1);
        }

        {
            // Test prefix --
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(
                MONGO_ODM_KEY(Bar::w) == 555,
                (--MONGO_ODM_KEY(Bar::x1), --MONGO_ODM_KEY(Bar::x2), --MONGO_ODM_CHILD(Bar, p, x)));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 - 1);
            REQUIRE(bar.value().x2.value() == initial_x2 - 1);
            REQUIRE(bar.value().p.x == initial_p_x - 1);
        }

        {
            // Test operator+=
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(MONGO_ODM_KEY(Bar::w) == 555,
                                       (MONGO_ODM_KEY(Bar::x1) += 5, MONGO_ODM_KEY(Bar::x2) += 5,
                                        MONGO_ODM_CHILD(Bar, p, x) += 5));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 + 5);
            REQUIRE(bar.value().x2.value() == initial_x2 + 5);
            REQUIRE(bar.value().p.x == initial_p_x + 5);
        }

        {
            // Test operator-=
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(MONGO_ODM_KEY(Bar::w) == 555,
                                       (MONGO_ODM_KEY(Bar::x1) -= 5, MONGO_ODM_KEY(Bar::x2) -= 5,
                                        MONGO_ODM_CHILD(Bar, p, x) -= 5));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 - 5);
            REQUIRE(bar.value().x2.value() == initial_x2 - 5);
            REQUIRE(bar.value().p.x == initial_p_x - 5);
        }

        {
            // Test operator*=
            initial_x1 = bar.value().x1;
            initial_x2 = bar.value().x2.value();
            initial_p_x = bar.value().p.x;
            auto res = coll.update_one(MONGO_ODM_KEY(Bar::w) == 555,
                                       (MONGO_ODM_KEY(Bar::x1) *= 5, MONGO_ODM_KEY(Bar::x2) *= 5,
                                        MONGO_ODM_CHILD(Bar, p, x) *= 5));
            REQUIRE(res);
            REQUIRE(res.value().modified_count() == 1);
            bar = Bar::find_one(MONGO_ODM_KEY(Bar::w) == 555);
            REQUIRE(bar);
            REQUIRE(bar.value().x1 == initial_x1 * 5);
            REQUIRE(bar.value().x2.value() == initial_x2 * 5);
            REQUIRE(bar.value().p.x == initial_p_x * 5);
        }
    }
}
