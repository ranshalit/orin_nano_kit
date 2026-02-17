/*
* Copyright 2014-2023 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "Offline.h"

#include <NvPerfHudDataModel.h>

#define RYML_SINGLE_HDR_DEFINE_NOW
#include <ryml_all.hpp>

#include <iostream>
#include "Offline_HudDataModel_SupportedChips.h"

namespace nv { namespace perf { namespace test {
    namespace {
#if defined (__aarch64__)
        static const char* exampleChip = "GA10B";
#else
        static const char* exampleChip = "GA102";
#endif
    } // namespace

    NVPW_TEST_SUITE_BEGIN("HudDataModel");

    // /// RingBuffer /////////////////////////////////////////////////////////

    NVPW_TEST_CASE("RingBuffer")
    {
        NVPW_SUBCASE("Empty")
        {
            hud::RingBuffer<int> buffer;
            NVPW_CHECK(buffer.Size() == 0);
            NVPW_CHECK(buffer.MaxSize() == 0);
        }

        NVPW_SUBCASE("Not Full")
        {
            hud::RingBuffer<int> buffer(4);
            buffer.Push( 5);
            buffer.Push(15);

            NVPW_CHECK(buffer.Size()       ==  2);
            NVPW_CHECK(buffer.MaxSize()    ==  4);
            NVPW_CHECK(buffer.WriteIndex() ==  2);
            NVPW_CHECK(buffer.Back()       ==  5);
            NVPW_CHECK(buffer.Front()      == 15);
            NVPW_CHECK(buffer.Get(0)       ==  5);
            NVPW_CHECK(buffer.Get(1)       == 15);
        }

        NVPW_SUBCASE("Full")
        {
            hud::RingBuffer<int> buffer(4);
            buffer.Push( 5);
            buffer.Push(15);
            buffer.Push(25);
            buffer.Push(35);
            buffer.Push( 6);

            NVPW_CHECK(buffer.Size()       ==  4);
            NVPW_CHECK(buffer.MaxSize()    ==  4);
            NVPW_CHECK(buffer.WriteIndex() ==  1);
            NVPW_CHECK(buffer.Back()       == 15);
            NVPW_CHECK(buffer.Front()      ==  6);
            NVPW_CHECK(buffer.Get(0)       == 15);
            NVPW_CHECK(buffer.Get(1)       == 25);
            NVPW_CHECK(buffer.Get(2)       == 35);
            NVPW_CHECK(buffer.Get(3)       ==  6);
        }
    }

    // /// Helper /////////////////////////////////////////////////////////////

    template<typename FromYamlFn>
    static void ParseYaml(const std::string yaml, FromYamlFn&& fn)
    {
        hud::ScopedRymlErrorHandler customRymlErrorHandler;

        ryml::Tree tree = ryml::parse_in_arena(ryml::csubstr(yaml.data(), yaml.size()));
        ryml::NodeRef root = tree.rootref();

        fn(root);
    }

    // /// Yaml Parsing Tests /////////////////////////////////////////////////

    NVPW_TEST_CASE("BoolFromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        bool default_;
        bool return_;
        auto parseBool = [&default_, &valid, &return_](const ryml::NodeRef& root) {
            ryml::NodeRef node;
            if (root.is_map()) // normal case
            {
                NVPW_REQUIRE(root.num_children() == 1);
                node = root.first_child();
            }
            else // only invalid cases
            {
                node = root;
            }

            return_ = hud::BoolFromYaml(node, default_, &valid);
        };

        // valid yaml = true
        default_ = false;
        for (const std::string& yaml : {"myBool: 1", "myBool: true", "myBool: True", "myBool: TRUE"})
        {
            ParseYaml(yaml, parseBool);
            NVPW_CHECK(valid == true);
            NVPW_CHECK(return_ == !default_);
        }

        // valid yaml = false
        default_ = true;
        for (const std::string& yaml : {"myBool: 0", "myBool: false", "myBool: False", "myBool: FALSE"})
        {
            ParseYaml(yaml, parseBool);
            NVPW_CHECK(valid == true);
            NVPW_CHECK(return_ == !default_);
        }

        // invalid yaml
        default_ = true;
        for (const std::string& yaml : { "", "asdf", "true", "myBool:", "myBool: asdf" })
        {
            ParseYaml(yaml, parseBool);
            NVPW_CHECK(valid == false);
            NVPW_CHECK(return_ == default_);
        }
    }

    NVPW_TEST_CASE("Color::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::Color return_;
        auto parseColor = [&valid, &return_](const ryml::NodeRef& root) {
            ryml::NodeRef node;
            if (root.is_map()) // normal case
            {
                NVPW_REQUIRE(root.num_children() == 1);
                node = root.first_child();
            }
            else // only invalid cases
            {
                node = root;
            }

            return_ = hud::Color::FromYaml(node, hud::Color::Invalid(), &valid);
        };

        // valid yaml = red, red with alpha
        for (const std::string& yaml : { "myColor: 0xff0000", "myColor: 0xff0000ff" })
        {
            ParseYaml(yaml, parseColor);
            NVPW_CHECK(valid == true);
            NVPW_CHECK(return_ == hud::Color::Red());
        }

        // invalid yaml, not keyval
        for (const std::string& yaml : { "", "asdf", "0xff0000", "0xff0000ff", "myColor:" })
        {
            ParseYaml(yaml, parseColor);
            NVPW_CHECK(valid == false);
            NVPW_CHECK(!return_.IsValid());
        }

        // invalid yaml, keyval
        for (const std::string& yaml : { "myColor: 0xff", "myColor: 0xff0000fff", "myColor: 1xff0000", "myColor: asdf" })
        {
            ParseYaml(yaml, parseColor);
            NVPW_CHECK(valid == false);
            NVPW_CHECK(!return_.IsValid());
        }
    }

    NVPW_TEST_CASE("StyledText::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::StyledText return_;
        auto parseStyledText = [&valid, &return_](const ryml::NodeRef& root) {
            ryml::NodeRef node;
            if (root.is_map() || root.is_seq()) // normal case
            {
                NVPW_REQUIRE(root.num_children() == 1);
                node = root.first_child();
            }
            else // only invalid cases
            {
                node = root;
            }

            return_ = hud::StyledText::FromYaml(node, hud::StyledText(), &valid);
        };

        // valid yaml
        ParseYaml("label: ", parseStyledText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.text.empty());

        ParseYaml("label: myText", parseStyledText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.text == "myText");

        ParseYaml(
            "label:\n"
            "  - text: myText", parseStyledText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.text == "myText");

        ParseYaml(
            "label:\n"
            "  - text: myText\n"
            "    color: 0xff0000", parseStyledText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.text == "myText");
        NVPW_CHECK(return_.color == hud::Color::Red());

        // invalid yaml
        for (const std::string& yaml : { "", "asdf", "label" })
        {
            ParseYaml(yaml, parseStyledText);
            NVPW_CHECK(valid == false);
            NVPW_CHECK(return_.text.empty());
        }

        // invalid multi-line yaml
        std::string invalidMultiline1 =
            "label:\n"
            "  text: myText\n"
            "  color: 0xff0000";
        std::string invalidMultiline2 =
            "label:\n"
            "  - text: myText\n"
            "  - color: 0xff0000";
        std::string invalidMultiline3 =
            "label:\n"
            "  - asdf: myText\n";
        for (const std::string& yaml : { invalidMultiline1, invalidMultiline2, invalidMultiline3 })
        {
            ParseYaml(yaml, parseStyledText);
            NVPW_CHECK(valid == false);
            NVPW_CHECK(return_.text.empty());
        }
    }

    NVPW_TEST_CASE("MetricSignal::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::MetricSignal return_;
        auto parseMetricSignal = [&valid, &return_](const ryml::NodeRef& root) {
            ryml::NodeRef node;
            if (root.is_map() || root.is_seq()) // normal case
            {
                NVPW_REQUIRE(root.num_children() == 1);
                node = root.first_child();
            }
            else // can also be valid
            {
                node = root;
            }

            return_ = hud::MetricSignal::FromYaml(node, &valid);
        };

        // valid ScalarText's "metric: myMetric" , TimePlot's metrics list: "myMetric"
        for (const std::string& yaml : { "metric: my_metric", "my_metric" })
        {
            ParseYaml(yaml, parseMetricSignal);
            NVPW_CHECK(valid == true);
            NVPW_CHECK(return_.label.text.empty());
            NVPW_CHECK(return_.metric == "my_metric");
        }
        
        // valid minimal multi-line yaml
        std::string validMinimalMultiline =
            "- metric: my_metric";
        ParseYaml(validMinimalMultiline, parseMetricSignal);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text.empty());
        NVPW_CHECK(return_.metric == "my_metric");
        
        // valid full multi-line yaml
        std::string validFullMultiline =
            "- label:\n"
            "    - text: myMetric\n"
            "      color: 0xff0000\n"
            "  description: myDescription\n"
            "  metric: my_metric\n"
            "  color: 0x00ff00\n"
            "  max: 2\n"
            "  multiplier: 3\n"
            "  unit: ms";
        ParseYaml(validFullMultiline, parseMetricSignal);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text == "myMetric");
        NVPW_CHECK(return_.label.color == hud::Color::Red());
        NVPW_CHECK(return_.metric == "my_metric");
        NVPW_CHECK(return_.description == "myDescription");
        NVPW_CHECK(return_.color == hud::Color::Green());
        NVPW_CHECK(return_.maxValue == 2.0);
        NVPW_CHECK(return_.multiplier == 3.0);
        NVPW_CHECK(return_.unit == "ms");
        
        // invalid single-line yaml
        for (const std::string& yaml : { "metric: ", "metric: a b", "", "a b" })
        {
            ParseYaml(yaml, parseMetricSignal);
            NVPW_CHECK(valid == false);
        }

        // invalid multiline yaml
        std::string invalidMultiline1 =
            "- label: myMetric";
        std::string invalidMultiline2 =
            "- label: myMetric\n"
            "  metric: ";
        for (const std::string& yaml : { invalidMultiline1, invalidMultiline2 })
        {
            ParseYaml(yaml, parseMetricSignal);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("Panel::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::Panel return_;
        auto parsePanel = [&valid, &return_](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            return_ = hud::Panel::FromYaml(root.first_child(), &valid);
        };

        std::string validMinimal =
            "- name: myPanel\n"
            "  widgets:\n"
            "    - type: Separator";
        ParseYaml(validMinimal, parsePanel);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.name == "myPanel");
        NVPW_CHECK(return_.label.text == "myPanel");
        NVPW_REQUIRE(return_.widgets.size() == 1);
        NVPW_CHECK(return_.widgets[0]->type == hud::Widget::Type::Separator);
        
        std::string validFull1 =
            "- name: myPanel\n"
            "  label: myLabel\n"
            "  defaultOpen: false\n"
            "  widgets:\n"
            "    - type: Separator";
        ParseYaml(validFull1, parsePanel);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.name == "myPanel");
        NVPW_CHECK(return_.label.text == "myLabel");
        NVPW_CHECK(return_.widgets.size() == 1);
        
        std::string validFull2 =
            "- name: myPanel\n"
            "  label:\n"
            "    - text: myLabel\n"
            "      color: 0xff0000\n"
            "  defaultOpen: false\n"
            "  widgets:\n"
            "    - type: Separator\n"
            "    - type: Separator";
        ParseYaml(validFull2, parsePanel);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.name == "myPanel");
        NVPW_CHECK(return_.label.text == "myLabel");
        NVPW_CHECK(return_.label.color == hud::Color::Red());
        NVPW_REQUIRE(return_.widgets.size() == 2);
        NVPW_CHECK(return_.widgets[0]->type == hud::Widget::Type::Separator);
        NVPW_CHECK(return_.widgets[1]->type == hud::Widget::Type::Separator);
        
        std::string invalid1 =
            "- name: myPanel";
        std::string invalid2 =
            "- name: myPanel\n"
            "  widgets:";
        std::string invalid3 =
            "- name: myPanel\n"
            "  widgets:\n"
            "    - type: ";
        std::string invalid4 =
            "- name: myPanel\n"
            "  widgets:\n"
            "    - type: asdf";
        std::string invalid5 = // wrong widget list
            "- name: myPanel\n"
            "  widgets:\n"
            "    - type: Separator\n"
            "      type: Separator";
        for (const std::string& yaml : { invalid1, invalid2, invalid3, invalid4, invalid5 })
        {
            ParseYaml(yaml, parsePanel);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("WidgetFromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        std::unique_ptr<hud::Widget> return_;
        auto parseWidget = [&valid, &return_](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            return_ = std::move(hud::WidgetFromYaml(root.first_child(), &valid));
        };

        ParseYaml("- type: ScalarText", parseWidget);
        NVPW_CHECK(return_->type == hud::Widget::Type::ScalarText);
        NVPW_CHECK(valid == false); // Don't need fully valid input. Just the correct type.
        ParseYaml("- type: Separator", parseWidget);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_->type == hud::Widget::Type::Separator);
        ParseYaml("- type: TimePlot", parseWidget);
        NVPW_CHECK(return_->type == hud::Widget::Type::TimePlot);
        NVPW_CHECK(valid == false); // Don't need fully valid input. Just the correct type.

        for (const std::string& yaml : { "- type: Panel", "- type: ", "- type: asdf" })
        {
            ParseYaml(yaml, parseWidget);
            NVPW_CHECK(!return_);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("ScalarText::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::ScalarText return_;
        auto parseScalarText = [&valid, &return_](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            std::unique_ptr<hud::Widget> pWidget = std::move(hud::WidgetFromYaml(root.first_child(), &valid));
            NVPW_REQUIRE(pWidget);
            NVPW_REQUIRE(pWidget->type == hud::Widget::Type::ScalarText);
            return_ = std::move(hud::ScalarText(*static_cast<hud::ScalarText*>(pWidget.get())));      
        };

        std::string validMinimal =
            "- type: ScalarText\n"
            "  label: myLabel\n"
            "  metric: my_metric";
        ParseYaml(validMinimal, parseScalarText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text == "myLabel");
        NVPW_CHECK(return_.signal.metric == "my_metric");
        NVPW_CHECK(return_.showValue == hud::ScalarText::ShowValue::JustValue);

        std::string validFull =
            "- type: ScalarText\n"
            "  label:\n"
            "    - text: myLabel\n"
            "  metric: my_metric\n"
            "  decimalPlaces: 3\n"
            "  showValue : Hide";
        ParseYaml(validFull, parseScalarText);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text == "myLabel");
        NVPW_CHECK(return_.signal.metric == "my_metric");
        NVPW_CHECK(return_.decimalPlaces == 3);
        NVPW_CHECK(return_.showValue == hud::ScalarText::ShowValue::Hide);

        std::string invalid1 =
            "- type: ScalarText";
        std::string invalid2 =
            "- type: ScalarText\n"
            "  metric: my_metric";
        std::string invalid3 =
            "- type: ScalarText\n"
            "  label: myLabel";
        for (const std::string& yaml : { invalid1, invalid2, invalid3 })
        {
            ParseYaml(yaml, parseScalarText);
            NVPW_CHECK(valid == false);
        }

        // there are no invalid uses of decimalPlace and showValue, because they only result in warnings
        std::string wrn1 = validMinimal + "\n"
            "  decimalPlaces: -1";
        std::string wrn2 = validMinimal + "\n"
            "  decimalPlaces: 2.5";
        std::string wrn3 = validMinimal + "\n"
            "  showValue: ";
        std::string wrn4 = validMinimal + "\n"
            "  showValue: asdf";
        for (const std::string& yaml : { wrn1, wrn2, wrn3, wrn4 })
        {
            ParseYaml(yaml, parseScalarText);
            NVPW_CHECK(valid == true);
            // check for defaults
            NVPW_CHECK(return_.decimalPlaces == 1);
            NVPW_CHECK(return_.showValue == hud::ScalarText::ShowValue::JustValue);
        }
    }

    NVPW_TEST_CASE("Separator::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;
    
        bool valid;
        hud::Separator return_;
        auto parseSeparator = [&valid, &return_](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            std::unique_ptr<hud::Widget> pWidget = std::move(hud::WidgetFromYaml(root.first_child(), &valid));
            NVPW_REQUIRE(pWidget);
            NVPW_REQUIRE(pWidget->type == hud::Widget::Type::Separator);
            return_ = std::move(hud::Separator(*static_cast<hud::Separator*>(pWidget.get())));
        };

        for (const std::string& yaml : { "- type: Separator" })
        {
            ParseYaml(yaml, parseSeparator);
            NVPW_CHECK(valid == true);
        }

        std::string invalid1 =
            "- type: Separator\n"
            "  asdf";
        std::string invalid2 =
            " - type: Separator\n"
            "   name: asdf";
        for (const std::string& yaml : { invalid1, invalid2 })
        {
            ParseYaml(yaml, parseSeparator);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("TimePlot::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::TimePlot return_;
        auto parseTimePlot = [&valid, &return_](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            std::unique_ptr<hud::Widget> pWidget = std::move(hud::WidgetFromYaml(root.first_child(), &valid));
            NVPW_REQUIRE(pWidget);
            NVPW_REQUIRE(pWidget->type == hud::Widget::Type::TimePlot);
            return_ = std::move(hud::TimePlot(*static_cast<hud::TimePlot*>(pWidget.get())));
        };

        std::string validMinimal =
            "- type: TimePlot\n"
            "  chartType: Overlay\n"
            "  metrics:\n"
            "  - my_metric1\n"
            "  - my_metric2";
        ParseYaml(validMinimal, parseTimePlot);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text.empty());
        NVPW_CHECK(return_.label.color == hud::Color::Invalid());
        NVPW_CHECK(return_.chartType == hud::TimePlot::ChartType::Overlay);
        NVPW_REQUIRE(return_.signals.size() == 2);
        NVPW_CHECK(return_.signals[0].metric == "my_metric1");
        NVPW_CHECK(return_.signals[1].metric == "my_metric2");

        std::string validFull =
            "- type: TimePlot\n"
            "  label: myLabel\n"
            "  unit: myUnit\n"
            "  chartType: Stacked\n"
            "  metrics:\n"
            "    - my_metric1\n"
            "    - my_metric2\n"
            "  valueMin: 1\n"
            "  valueMax: 5";
        ParseYaml(validFull, parseTimePlot);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.label.text == "myLabel");
        NVPW_CHECK(return_.unit == "myUnit");
        NVPW_CHECK(return_.chartType == hud::TimePlot::ChartType::Stacked);
        NVPW_CHECK(return_.signals.size() == 2);
        NVPW_CHECK(return_.valueMin == 1.0);
        NVPW_CHECK(return_.valueMax == 5.0);

        std::string invalid1 = validMinimal + "\n"
            "  valueMin: \n"
            "  valueMax: ";
        std::string invalid2 = validMinimal + "\n"
            "  valueMin: asdf\n"
            "  valueMax: asdf";
        std::string invalid3 = validMinimal + "\n"
            "  valueMax: ";
        std::string invalid4 = validMinimal + "\n"
            "  valueMax: asdf";
        for (const std::string& yaml : { invalid1, invalid2, invalid3, invalid4 })
        {
            ParseYaml(yaml, parseTimePlot);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("HudConfiguration::FromYaml")
    {
        ScopedNvPerfLogDisabler logDisabler;

        bool valid;
        hud::HudConfiguration return_;
        std::vector<hud::Panel> panels
        {
            hud::Panel("myPanel1", hud::StyledText(), true, {}),
            hud::Panel("myPanel2", hud::StyledText(), true, {}),
            hud::Panel("myPanel3", hud::StyledText(), true, {}),
        };
        auto parseHudConfiguration = [&valid, &return_, &panels ](const ryml::NodeRef& root) {
            NVPW_REQUIRE(root.is_seq());
            NVPW_REQUIRE(root.num_children() == 1);
            return_ = hud::HudConfiguration::FromYaml(root.first_child(), panels, &valid);
        };

        std::string validMinimal =
            "- name: myConfig\n"
            "  speed: Low\n"
            "  panels:\n"
            "    - myPanel1\n"
            "    - myPanel2";
        ParseYaml(validMinimal, parseHudConfiguration);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.name == "myConfig");
        NVPW_REQUIRE(return_.panels.size() == 2);
        NVPW_REQUIRE(return_.samplingSpeed == hud::HudConfiguration::SamplingSpeed::Low);
        NVPW_CHECK(return_.panels[0].name == "myPanel1");
        NVPW_CHECK(return_.panels[1].name == "myPanel2");

        std::string validPanelNameAndDefaultOpen =
            "- name: myConfig\n"
            "  speed: Low\n"
            "  panels:\n"
            "    - name: myPanel1\n"
            "    - myPanel2\n"
            "    - name: myPanel3\n"
            "      defaultOpen: false";
        ParseYaml(validPanelNameAndDefaultOpen, parseHudConfiguration);
        NVPW_CHECK(valid == true);
        NVPW_CHECK(return_.name == "myConfig");
        NVPW_REQUIRE(return_.panels.size() == 3);
        NVPW_REQUIRE(return_.samplingSpeed == hud::HudConfiguration::SamplingSpeed::Low);
        NVPW_CHECK(return_.panels[0].name == "myPanel1");
        NVPW_CHECK(return_.panels[0].defaultOpen == true);
        NVPW_CHECK(return_.panels[1].name == "myPanel2");
        NVPW_CHECK(return_.panels[1].defaultOpen == true);
        NVPW_CHECK(return_.panels[2].name == "myPanel3");
        NVPW_CHECK(return_.panels[2].defaultOpen == false);

        std::string invalid1 =
            "- name: myConfig";
        std::string invalid2 =
            "- name: myConfig\n"
            "  speed: Low";
        std::string invalid3 =
            "- name: myConfig\n"
            "  panels:\n"
            "    - myPanel1\n"
            "    - myPanel2";
        std::string invalid4 =
            "- name: myConfig\n"
            "  speed: Low\n"
            "  panels:\n"
            "    - myPanel1\n"
            "      myPanel2"; // missing "-"
        std::string invalid5 =
            "- name: myConfig\n"
            "  speed: Low\n"
            "  panels:\n"
            "    - myPanel1\n"
            "    - nonExistantPanel";
        std::string invalid6 =
            "- name: myConfig\n"
            "  speed: asdf\n"
            "  panels:\n"
            "    - myPanel1\n"
            "      myPanel2";
        for (const std::string& yaml : { invalid1, invalid2, invalid3, invalid4, invalid5, invalid6 })
        {
            ParseYaml(yaml, parseHudConfiguration);
            NVPW_CHECK(valid == false);
        }
    }

    NVPW_TEST_CASE("HudPresets")
    {
        NVPW_SUBCASE("Initialize Invalid Chip")
        {
            ScopedNvPerfLogDisabler logDisabler;
            hud::HudPresets presets;
            NVPW_CHECK(!presets.Initialize("InvalidChip"));
        }

        NVPW_SUBCASE("Get Invalid Preset")
        {
            ScopedNvPerfLogDisabler logDisabler;
            hud::HudPresets presets;
            NVPW_REQUIRE(presets.Initialize(exampleChip));
            const auto& preset = presets.GetPreset("Invalid Preset");
            NVPW_CHECK(preset.name.empty());
            NVPW_CHECK(preset.chipName.empty());
            NVPW_CHECK(preset.pYaml == nullptr);
            NVPW_CHECK(preset.fileName.empty());
        }
        
        NVPW_SUBCASE("Initialize")
        {
            for (const char* chip : supportedChips)
            {
                NVPW_SUBCASE(chip)
                {
                    hud::HudPresets presets;
                    NVPW_REQUIRE(presets.Initialize(chip));
                    for (const hud::HudPreset& preset : presets.GetPresets())
                    {
                        NVPW_CHECK(!preset.name.empty());
                        NVPW_CHECK(preset.pYaml);
                    }
                }
            }
        }

        NVPW_SUBCASE("LoadFromString")
        {
            ScopedNvPerfLogDisabler logDisabler;

            std::string valid =
                "configurations:\n"
                "  - name: myConfig1\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myPanel1\n"
                "  - name: myConfig2\n"
                "    speed: High\n"
                "    panels:\n"
                "      - myPanel2";
            hud::HudPresets presets;
            NVPW_REQUIRE(presets.Initialize(exampleChip));
            NVPW_CHECK(presets.LoadFromString(valid.c_str(), "valid.yaml"));
            NVPW_REQUIRE(presets.GetPreset("myConfig1").IsValid());
            NVPW_REQUIRE(presets.GetPreset("myConfig2").IsValid());

            std::string invalidNoConfig =
                "panels:\n"
                "  - name: myPanel1\n"
                "    widgets:\n"
                "      - type: Separator";
            NVPW_CHECK(!presets.LoadFromString(invalidNoConfig.c_str(), "invalidNoConfig.yaml"));

            std::string invalidTwoConfigSections =
                "configurations:\n"
                "  - name: myConfig3\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myPanel1\n"
                "configurations:\n"
                "  - name: myConfig4\n"
                "    speed: High\n"
                "    panels:\n"
                "      - myPanel2";
            NVPW_CHECK(!presets.LoadFromString(invalidTwoConfigSections.c_str(), "invalidTwoConfigSections.yaml"));
        }
    }

    NVPW_TEST_CASE("HudDataModel")
    {
        NVPW_SUBCASE("Initialize All Presets")
        {
            for (const char* chip : supportedChips)
            {
                NVPW_SUBCASE(chip)
                {
                    hud::HudPresets presets;
                    NVPW_REQUIRE(presets.Initialize(chip));
                    for (const hud::HudPreset& preset : presets.GetPresets())
                    {
                        NVPW_SUBCASE(preset.name.c_str())
                        {
                            hud::HudDataModel model;
                            NVPW_REQUIRE(model.Load(preset));
                            NVPW_CHECK(model.Initialize(4, 1 / 60.0));
                        }
                    }
                }
            }
        }

        NVPW_SUBCASE("Load+Initialize+AddSample & Fail Trying to Add another Config")
        {
            ScopedNvPerfLogDisabler logDisabler;

            std::string valid1 =
                "panels:\n"
                "  - name: myPanel1\n"
                "    widgets:\n"
                "      - type: ScalarText\n"
                "        label: myLabel\n"
                "        metric:\n"
                "          - name: my_scalar_metric\n"
                "            metric: gpu__time_duration.sum\n"
                "            multiplier: 2\n"
                "  - name: myPanel2\n"
                "    widgets:\n"
                "      - type: TimePlot\n"
                "        label: myLabel\n"
                "        chartType: Overlay\n"
                "        metrics:\n"
                "        - gr__cycles_active.sum\n"
                "configurations:\n"
                "  - name: myConfig1\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myPanel1\n"
                "  - name: myConfig2\n"
                "    speed: High\n"
                "    panels:\n"
                "      - myPanel2";
            hud::HudPresets presets;
            NVPW_REQUIRE(presets.Initialize(exampleChip));
            NVPW_REQUIRE(presets.LoadFromString(valid1.c_str(), "valid1.yaml"));
            const hud::HudPreset& preset1 = presets.GetPreset("myConfig1");
            const hud::HudPreset& preset2 = presets.GetPreset("myConfig2");
            NVPW_REQUIRE(preset1.IsValid());
            NVPW_REQUIRE(preset2.IsValid());

            hud::HudDataModel model;
            NVPW_REQUIRE(model.Load(preset1));
            NVPW_REQUIRE(model.Load(preset2));
            NVPW_REQUIRE(model.GetConfigurations().size() == 2);
            NVPW_REQUIRE(model.GetConfigurations()[0].panels.size() == 1);
            NVPW_REQUIRE(model.GetConfigurations()[0].panels[0].widgets.size() == 1);
            NVPW_REQUIRE(model.GetConfigurations()[0].panels[0].widgets[0]);
            NVPW_REQUIRE(model.GetConfigurations()[1].panels.size() == 1);
            NVPW_REQUIRE(model.GetConfigurations()[1].panels[0].widgets.size() == 1);
            NVPW_REQUIRE(model.GetConfigurations()[1].panels[0].widgets[0]);

            const auto& scalarText = *static_cast<hud::ScalarText*>(model.GetConfigurations()[0].panels[0].widgets[0].get());
            const auto& scalarSignal = scalarText.signal;
            const auto& timePlot = *static_cast<hud::TimePlot*>(model.GetConfigurations()[1].panels[0].widgets[0].get());
            NVPW_REQUIRE(timePlot.signals.size() == 1);
            const auto& timeSignal = timePlot.signals[0];

            NVPW_REQUIRE(model.Initialize(1.0, 10.0));
            NVPW_CHECK(scalarText.signal.maxNumSamples == 1);
            NVPW_REQUIRE(timePlot.signals[0].maxNumSamples >= 10);

            model.AddSample(1.0, { 1.0, 2.0 });
            model.AddFrameLevelValues(1, { 4.0, 3.0 });
            NVPW_CHECK(timeSignal.valBuffer.Front() == 2.0);
            NVPW_CHECK(scalarSignal.valBuffer.Front() == 4.0 * 2.0); // with multiplier

            // Fail adding another configuration
            std::string valid2 =
                "panels:\n"
                "  - name: myPanel3\n"
                "    widgets:\n"
                "      - type: ScalarText\n"
                "        label: myLabel\n"
                "        metric: my_scalar_metric2\n"
                "configurations:\n"
                "  - name: myConfig3\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myPanel3";
            NVPW_REQUIRE(presets.LoadFromString(valid2.c_str(), "valid2.yaml"));
            const hud::HudPreset& preset3 = presets.GetPreset("myConfig3");
            NVPW_REQUIRE(preset3.IsValid());
            NVPW_REQUIRE(!model.Load(preset3));
        }

        NVPW_SUBCASE("Invalid Panel")
        {
            ScopedNvPerfLogDisabler logDisabler;

            std::string invalidPanel =
                "panels:\n"
                "  - name:\n"
                "configurations:\n"
                "  - name: myConfig1\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myPanel1\n";
            hud::HudPresets presets;
            NVPW_REQUIRE(presets.Initialize(exampleChip));
            NVPW_REQUIRE(presets.LoadFromString(invalidPanel.c_str(), "invalidPanel.yaml"));
            NVPW_REQUIRE(presets.GetPreset("myConfig1").IsValid());

            hud::HudDataModel model;
            NVPW_CHECK(!model.Load(presets.GetPreset("myConfig1")));
        }

        NVPW_SUBCASE("Missing Panel")
        {
            ScopedNvPerfLogDisabler logDisabler;

            std::string missingPanel =
                "panels:\n"
                "  - name: myPanel1\n"
                "    widgets:\n"
                "      - type: ScalarText\n"
                "        label: myLabel\n"
                "        metric: my_metric\n"
                "configurations:\n"
                "  - name: myConfig1\n"
                "    speed: Low\n"
                "    panels:\n"
                "      - myMissingPanel\n";
            hud::HudPresets presets;
            NVPW_REQUIRE(presets.Initialize(exampleChip));
            NVPW_REQUIRE(presets.LoadFromString(missingPanel.c_str(), "invalidPanel.yaml"));
            NVPW_REQUIRE(presets.GetPreset("myConfig1").IsValid());

            hud::HudDataModel model;
            NVPW_CHECK(!model.Load(presets.GetPreset("myConfig1")));
        }
    }

    NVPW_TEST_SUITE_END();

}}} // nv::perf::test