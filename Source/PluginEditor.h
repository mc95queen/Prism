#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// === 1. 커스텀 디자인 클래스 (노브 & 버튼 꾸미기) ===
class PrismLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PrismLookAndFeel()
    {
        // 컬러 팔레트 정의 (다크 모드 + 시안 블루)
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff2a2a2a)); // 노브 몸체
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00b5ff)); // 게이지 색상 (Cyan)
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1a1a1a)); // 배경 트랙
        
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff151515)); // 버튼 기본색
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00b5ff)); // 버튼 켜졌을 때 (Cyan)
        setColour(juce::TextButton::textColourOnId, juce::Colours::black); // 켜졌을 때 글자색
        setColour(juce::TextButton::textColourOffId, juce::Colours::grey); // 꺼졌을 때 글자색
    }

    // 노브 그리기 함수 (메탈릭 디자인)
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. 배경 트랙 (어두운 회색)
        g.setColour(findColour(juce::Slider::rotarySliderOutlineColourId));
        g.drawEllipse(rx, ry, rw, rw, 1.0f); // 얇은 테두리
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff111111));
        g.strokePath(backgroundArc, juce::PathStrokeType(radius * 0.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. 값 게이지 (Cyan 색상)
        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(valueArc, juce::PathStrokeType(radius * 0.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. 노브 몸통 (메탈릭 그라데이션)
        auto knobRadius = radius * 0.75f; // 트랙보다 작게
        juce::ColourGradient gradient(juce::Colour(0xff444444), centreX, centreY - knobRadius,
                                      juce::Colour(0xff111111), centreX, centreY + knobRadius, false);
        g.setGradientFill(gradient);
        g.fillEllipse(centreX - knobRadius, centreY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        
        // 노브 테두리 (입체감)
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(centreX - knobRadius, centreY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // 4. 포인터 (지침)
        juce::Path p;
        auto pointerLength = knobRadius * 0.7f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId).brighter()); // 밝은 Cyan
        g.fillPath(p);
    }
};

// === 2. 에디터 클래스 ===
class PrismAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PrismAudioProcessorEditor (PrismAudioProcessor&);
    ~PrismAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    // 타이머 콜백: 버튼 상태를 파라미터와 동기화
    void timerCallback() override;

private:
    PrismAudioProcessor& audioProcessor;

    // 커스텀 룩앤필 객체
    PrismLookAndFeel prismLookAndFeel;

    // UI 컴포넌트
    juce::Slider inputSlider, outputSlider, mixSlider;
    juce::Label inputLabel, outputLabel, mixLabel;
    
    // 5개의 알고리즘 버튼
    juce::TextButton algoButtons[5];
    juce::String algoNames[5] = { "Tanh", "ArcTan", "Hard Clip", "Sigmoid", "Sine Fold" };

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputAttach;
    std::unique_ptr<SliderAttachment> outputAttach;
    std::unique_ptr<SliderAttachment> mixAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismAudioProcessorEditor)
};
