#include "PluginProcessor.h"
#include "PluginEditor.h"

PrismAudioProcessorEditor::PrismAudioProcessorEditor (PrismAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (500, 350); // 창 크기 살짝 키움

    // 룩앤필 적용 (전역 혹은 이 컴포넌트에만)
    juce::LookAndFeel::setDefaultLookAndFeel(&prismLookAndFeel);

    // === 슬라이더 설정 ===
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, juce::String name)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setFont(juce::Font(14.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(inputSlider, inputLabel, "INPUT");
    setupSlider(outputSlider, outputLabel, "OUTPUT");
    setupSlider(mixSlider, mixLabel, "MIX");

    // === 버튼 5개 설정 ===
    for (int i = 0; i < 5; ++i)
    {
        algoButtons[i].setButtonText(algoNames[i]);
        algoButtons[i].setClickingTogglesState(true); // 토글 가능
        algoButtons[i].setRadioGroupId(1001); // 같은 그룹 ID -> 하나만 선택됨
        algoButtons[i].setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight); // 버튼끼리 붙이기
        
        // 버튼 클릭 시 파라미터 값 변경 (Normalized Value: 0.0 ~ 1.0)
        // Choice 파라미터는 0.0, 0.25, 0.5, 0.75, 1.0 순서로 매핑됨 (항목이 5개일 때)
        algoButtons[i].onClick = [this, i] {
            auto* param = audioProcessor.apvts.getParameter("algo");
            if (param != nullptr)
            {
                // Choice Parameter의 값을 인덱스에 맞게 설정 (Range 변환)
                float value = (float)i / 4.0f; // 5개니까 0/4, 1/4, ... 4/4
                param->setValueNotifyingHost(value);
            }
        };
        
        addAndMakeVisible(algoButtons[i]);
    }
    
    // 첫 번째와 마지막 버튼 둥근 모서리 처리
    algoButtons[0].setConnectedEdges(juce::Button::ConnectedOnRight);
    algoButtons[4].setConnectedEdges(juce::Button::ConnectedOnLeft);

    // === Attachments 연결 ===
    inputAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "input", inputSlider);
    outputAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "output", outputSlider);
    mixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);
    
    // 타이머 시작 (버튼 상태 동기화용, 30ms마다 체크)
    startTimer(30);
}

PrismAudioProcessorEditor::~PrismAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr); // 룩앤필 해제 (필수)
}

// 타이머: 파라미터 값이 바뀌면(예: 프리셋 로드) 버튼 상태도 따라가게 함
void PrismAudioProcessorEditor::timerCallback()
{
    auto* param = audioProcessor.apvts.getParameter("algo");
    if (param)
    {
        // 현재 선택된 알고리즘 인덱스 가져오기 (0 ~ 4)
        // convertFrom0to1을 사용하여 정규화된 값을 실제 인덱스로 변환
        int index = std::round(param->convertFrom0to1(param->getValue()));
        
        // 해당 버튼만 켜기 (무한 루프 방지를 위해 상태가 다를 때만 set)
        if (index >= 0 && index < 5)
        {
            if (algoButtons[index].getToggleState() == false)
            {
                algoButtons[index].setToggleState(true, juce::dontSendNotification);
            }
        }
    }
}

void PrismAudioProcessorEditor::paint (juce::Graphics& g)
{
    // === 배경 그라데이션 (다크) ===
    g.fillAll(juce::Colour(0xff0d0d0d)); // 완전 어두운 배경

    // 상단 타이틀
    g.setColour(juce::Colour(0xff00b5ff)); // Cyan
    g.setFont(juce::Font("Futura", 24.0f, juce::Font::bold));
    g.drawFittedText("PRISM", getLocalBounds().removeFromTop(50), juce::Justification::centred, 1);
    
    // 섹션 구분선 (선택 사항)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(120, 20.0f, getWidth() - 20.0f);
    
    // ==========================================================
    // ▼ [추가된 부분] 하단 제작자 크레딧 (Designed by Rosid)
    // ==========================================================
    g.setColour(juce::Colours::white.withAlpha(0.4f)); // 투명도 40% (은은하게)
    g.setFont(12.0f); // 작은 폰트

    // 전체 화면의 맨 아래 20px 영역에 글씨를 씁니다.
    auto creditArea = getLocalBounds().removeFromBottom(20);
    g.drawText("Designed by Rosid", creditArea, juce::Justification::centred, true);
}

void PrismAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // 타이틀 공간

    // === 버튼 배치 (상단) ===
    auto btnArea = area.removeFromTop(40);
    btnArea = btnArea.reduced(20, 0); // 좌우 여백
    
    int btnWidth = btnArea.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        algoButtons[i].setBounds(btnArea.removeFromLeft(btnWidth));
    }

    area.removeFromTop(40); // 간격

    // === 노브 배치 (하단) ===
    auto knobArea = area;
    int knobWidth = knobArea.getWidth() / 3;

    auto inputArea = knobArea.removeFromLeft(knobWidth);
    inputLabel.setBounds(inputArea.removeFromTop(30));
    inputSlider.setBounds(inputArea.reduced(10)); // 노브끼리 간격 확보

    auto mixArea = knobArea.removeFromLeft(knobWidth);
    mixLabel.setBounds(mixArea.removeFromTop(30));
    mixSlider.setBounds(mixArea.reduced(10));

    auto outputArea = knobArea;
    outputLabel.setBounds(outputArea.removeFromTop(30));
    outputSlider.setBounds(outputArea.reduced(10));
}
