#include "dive_plan_gui.hpp"
#include "main_gui.hpp"
#include "table_helper.hpp"

namespace DiveComputer {

void DivePlanWindow::setupSummaryWidget() {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Create layout for the summary widget
    QVBoxLayout* summaryLayout = qobject_cast<QVBoxLayout*>(summaryTable->layout());
    if (!summaryLayout) {
        summaryLayout = new QVBoxLayout(summaryTable);
        summaryTable->setLayout(summaryLayout);
    }
    
    // Clear any existing content
    QLayoutItem* child;
    while ((child = summaryLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    
    // Create a form layout for the summary data
    QFormLayout* formLayout = new QFormLayout();
    formLayout->setContentsMargins(10, 5, 10, 5);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(5);
    
    // Add label for dive number with explicit style
    QLabel* diveNumberTitle = new QLabel("Dive Number:", summaryTable);
    diveNumberTitle->setStyleSheet(PLAIN_STYLE);
    QLabel* diveNumberLabel = new QLabel(QString::number(m_divePlan->m_diveNumber), summaryTable);
    diveNumberLabel->setStyleSheet(PLAIN_STYLE);
    formLayout->addRow(diveNumberTitle, diveNumberLabel);
    
    // Add nofly time with explicit style
    QLabel* noflyTimeTitle = new QLabel("NoFly Time:", summaryTable);
    noflyTimeTitle->setStyleSheet(PLAIN_STYLE);
    noflyTimeLabel = new QLabel(QString::number(m_divePlan->getNoFlyTime(), 'f', 0) + " hours", summaryTable);
    noflyTimeLabel->setStyleSheet(PLAIN_STYLE);
    formLayout->addRow(noflyTimeTitle, noflyTimeLabel);

    // Add GF values (editable)
    QWidget* gfWidget = new QWidget(summaryTable);
    gfWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* gfLayout = new QHBoxLayout(gfWidget);
    gfLayout->setContentsMargins(0, 0, 0, 0);
    gfLayout->setSpacing(5);
    
    gfLowEdit = new QLineEdit(QString::number(g_parameters.m_gf[0]), gfWidget);
    gfHighEdit = new QLineEdit(QString::number(g_parameters.m_gf[1]), gfWidget);
    
    // Set validators for GF values (0-100)
    QIntValidator* gfValidator = new QIntValidator(0, 100, gfWidget);
    gfLowEdit->setValidator(gfValidator);
    gfHighEdit->setValidator(gfValidator);
    
    // Set fixed width for better appearance
    gfLowEdit->setFixedWidth(40);
    gfHighEdit->setFixedWidth(40);

    // Apply custom style directly - don't use setFrame
    gfLowEdit->setStyleSheet(EDITABLE_STYLE);  
    gfLowEdit->setAlignment(Qt::AlignCenter);
    gfHighEdit->setStyleSheet(EDITABLE_STYLE); 
    gfHighEdit->setAlignment(Qt::AlignCenter);

    // Add slash label between values (with PLAIN_STYLE to ensure no borders)
    QLabel* slashLabel = new QLabel("/", gfWidget);
    slashLabel->setStyleSheet(PLAIN_STYLE); // Apply PLAIN_STYLE to ensure no borders
    
    // Assemble GF layout
    gfLayout->addWidget(gfLowEdit);
    gfLayout->addWidget(slashLabel);
    gfLayout->addWidget(gfHighEdit);
    gfLayout->addStretch();
    
    // Add GF title with explicit style
    QLabel* gfTitle = new QLabel("GF:", summaryTable);
    gfTitle->setStyleSheet(PLAIN_STYLE);
    formLayout->addRow(gfTitle, gfWidget);
    
    // Connect signals for GF edits
    connect(gfLowEdit, &QLineEdit::editingFinished, this, &DivePlanWindow::onGFChanged);
    connect(gfHighEdit, &QLineEdit::editingFinished, this, &DivePlanWindow::onGFChanged);
    
    // Add TTS Target with explicit style for both label and value
    QLabel* ttsTargetTitle = new QLabel("TTS Target:", summaryTable);
    ttsTargetTitle->setStyleSheet(PLAIN_STYLE);
    ttsTargetLabel = new QLabel("-- min", summaryTable);
    ttsTargetLabel->setStyleSheet(PLAIN_STYLE);
    formLayout->addRow(ttsTargetTitle, ttsTargetLabel);
    
    // Add Max TTS with explicit style for both label and value
    QLabel* maxTtsTitle = new QLabel("TTS Max:", summaryTable);
    maxTtsTitle->setStyleSheet(PLAIN_STYLE);
    
    // Create a container widget for consistent visibility management
    QWidget* maxTtsWidget = new QWidget(summaryTable);
    maxTtsWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* maxTtsLayout = new QHBoxLayout(maxTtsWidget);
    maxTtsLayout->setContentsMargins(0, 0, 0, 0);
    maxTtsLayout->setSpacing(0);
    
    maxTtsLabel = new QLabel("-- min", maxTtsWidget);
    maxTtsLabel->setStyleSheet(PLAIN_STYLE);
    maxTtsLayout->addWidget(maxTtsLabel);
    
    formLayout->addRow(maxTtsTitle, maxTtsWidget);
    
    // Store the title and the LABEL (not the widget) for value updates
    m_summaryLabels.append(qMakePair(maxTtsTitle, maxTtsLabel));
    
    // Create a new list to track widget containers for visibility
    // This avoids the type mismatch in m_summaryLabels
    m_summaryWidgets.append(qMakePair(maxTtsTitle, maxTtsWidget));
    
    // Add Max Time with explicit style for both label and value
    QLabel* maxTimeTitle = new QLabel("Max BT:", summaryTable);
    maxTimeTitle->setStyleSheet(PLAIN_STYLE);
    
    QWidget* maxTimeWidget = new QWidget(summaryTable);
    maxTimeWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* maxTimeLayout = new QHBoxLayout(maxTimeWidget);
    maxTimeLayout->setContentsMargins(0, 0, 0, 0);
    maxTimeLayout->setSpacing(0);
    
    maxTimeLabel = new QLabel("-- min", maxTimeWidget);
    maxTimeLabel->setStyleSheet(PLAIN_STYLE);
    maxTimeLayout->addWidget(maxTimeLabel);
    
    formLayout->addRow(maxTimeTitle, maxTimeWidget);
    
    // Store both the label for value updates and the widget for visibility
    m_summaryLabels.append(qMakePair(maxTimeTitle, maxTimeLabel));
    m_summaryWidgets.append(qMakePair(maxTimeTitle, maxTimeWidget));
    
    // Add TTS Delta with explicit style for both label and value
    QLabel* ttsDeltaTitle = new QLabel("\u0394 TTS +5min:", summaryTable);
    ttsDeltaTitle->setStyleSheet(PLAIN_STYLE);
    ttsDeltaLabel = new QLabel("-- min", summaryTable);
    ttsDeltaLabel->setStyleSheet(PLAIN_STYLE);
    formLayout->addRow(ttsDeltaTitle, ttsDeltaLabel);
    
    // Add AP (conditionally shown) with bar unit on the right
    QLabel* apLabelTitle = new QLabel("AP:", summaryTable);
    apLabelTitle->setStyleSheet(PLAIN_STYLE);
    
    // Create widget to hold AP value and unit
    QWidget* apWidget = new QWidget(summaryTable);
    apWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* apLayout = new QHBoxLayout(apWidget);
    apLayout->setContentsMargins(0, 0, 0, 0);
    apLayout->setSpacing(0); // Reduced spacing to make it look like one field
    
    // Create a label for AP value directly (non-editable)
    apLabel = new QLabel("-- bar", apWidget);
    apLabel->setStyleSheet(PLAIN_STYLE);
    apLayout->addWidget(apLabel);
    
    formLayout->addRow(apLabelTitle, apWidget);
    
    // Store both the label for value updates and the widget for visibility
    m_summaryLabels.append(qMakePair(apLabelTitle, apLabel));
    m_summaryWidgets.append(qMakePair(apLabelTitle, apWidget));
    
    // Create the mission input (editable, conditionally shown) with min unit on the right
    QLabel* missionLabelTitle = new QLabel("Mission:", summaryTable);
    missionLabelTitle->setStyleSheet(PLAIN_STYLE);
    
    // Create a widget to hold the mission edit field and unit label
    QWidget* missionWidget = new QWidget(summaryTable);
    missionWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* missionLayout = new QHBoxLayout(missionWidget);
    missionLayout->setContentsMargins(0, 0, 0, 0);
    missionLayout->setSpacing(0); // Reduced spacing to make it look like one field
    
    // Create the edit field with border
    missionEdit = new QLineEdit("0", missionWidget);
    missionEdit->setValidator(new QDoubleValidator(0, 10000, 0, missionEdit));
    missionEdit->setFixedWidth(40);
    missionEdit->setStyleSheet(EDITABLE_STYLE);
    missionEdit->setAlignment(Qt::AlignCenter);

    // Create the unit label with PLAIN_STYLE (no border)
    QLabel* missionUnitLabel = new QLabel("min", missionWidget);
    missionUnitLabel->setStyleSheet(PLAIN_STYLE);
    
    // Assemble the layout
    missionLayout->addWidget(missionEdit);
    missionLayout->addWidget(missionUnitLabel);
    missionLayout->addStretch();
    
    formLayout->addRow(missionLabelTitle, missionWidget);
    m_summaryEdits.append(qMakePair(missionLabelTitle, missionEdit));
    
    // Connect mission edit signal
    connect(missionEdit, &QLineEdit::editingFinished, this, &DivePlanWindow::onMissionChanged);
    
    // Add Turn TTS (conditionally shown)
    QLabel* turnTtsLabelTitle = new QLabel("Turn TTS:", summaryTable);
    turnTtsLabelTitle->setStyleSheet(PLAIN_STYLE);
    
    QWidget* turnTtsWidget = new QWidget(summaryTable);
    turnTtsWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* turnTtsLayout = new QHBoxLayout(turnTtsWidget);
    turnTtsLayout->setContentsMargins(0, 0, 0, 0);
    turnTtsLayout->setSpacing(0);
    
    turnTtsLabel = new QLabel("-- min", turnTtsWidget);
    turnTtsLabel->setStyleSheet(PLAIN_STYLE);
    turnTtsLayout->addWidget(turnTtsLabel);
    
    formLayout->addRow(turnTtsLabelTitle, turnTtsWidget);
    
    // Store both the label for value updates and the widget for visibility
    m_summaryLabels.append(qMakePair(turnTtsLabelTitle, turnTtsLabel));
    m_summaryWidgets.append(qMakePair(turnTtsLabelTitle, turnTtsWidget));
    
    // Add TP with bar unit on the right (conditionally shown)
    QLabel* tpLabelTitle = new QLabel("TP:", summaryTable);
    tpLabelTitle->setStyleSheet(PLAIN_STYLE);
    
    // Create widget to hold TP value and unit
    QWidget* tpWidget = new QWidget(summaryTable);
    tpWidget->setStyleSheet(PLAIN_STYLE);
    QHBoxLayout* tpLayout = new QHBoxLayout(tpWidget);
    tpLayout->setContentsMargins(0, 0, 0, 0);
    tpLayout->setSpacing(0); // Reduced spacing to make it look like one field
    
    tpLabel = new QLabel("-- bar", tpWidget);
    tpLabel->setStyleSheet(PLAIN_STYLE);    
    tpLayout->addWidget(tpLabel);
    
    formLayout->addRow(tpLabelTitle, tpWidget);
    
    // Store both the label for value updates and the widget for visibility
    m_summaryLabels.append(qMakePair(tpLabelTitle, tpLabel));
    m_summaryWidgets.append(qMakePair(tpLabelTitle, tpWidget));
    
    // Add the form layout to the summary layout
    summaryLayout->addLayout(formLayout);
    
    // Add stretch to push everything to the top
    summaryLayout->addStretch();
    
    // Monitor performance
    logWrite("DivePlanWindow::setupSummaryTable() took ", timer.elapsed(), " ms");

    // Update the dive summary
    refreshDiveSummaryTable();
}

void DivePlanWindow::refreshDiveSummaryTable() {
    // Log performance
    QElapsedTimer timer;
    timer.start();
    
    // Update nofly time
    noflyTimeLabel->setText(QString::number(m_divePlan->getNoFlyTime(), 'f', 0) + " hours");

    // Update TTS Target - always visible
    ttsTargetLabel->setText(QString::number(m_divePlan->m_tts, 'f', 0) + " min");
    ttsDeltaLabel->setText(QString::number(m_divePlan->m_ttsDelta, 'f', 0) + " min");
    
    // Show/hide and update all AP-dependent elements
    bool showAP = (m_divePlan->m_mode == diveMode::OC) || 
                  (m_divePlan->m_mode == diveMode::CC && m_divePlan->m_bailout);

    // Process all elements using their titles
    const QStringList apDependentTitles = {"Max BT:", "TTS Max:", "AP:"};
    
    // Update visibility of container widgets first
    for (const auto& pair : m_summaryWidgets) {
        if (apDependentTitles.contains(pair.first->text())) {
            // Set visibility for both the title and its container widget
            pair.first->setVisible(showAP);
            pair.second->setVisible(showAP);
        }
    }
    
    // Update values for labels
    for (const auto& pair : m_summaryLabels) {
        if (apDependentTitles.contains(pair.first->text())) {
            // Update values if visible
            if (showAP) {
                if (pair.first->text() == "Max BT:") {
                    pair.second->setText(QString::number(m_divePlan->m_maxResult.first, 'f', 0) + " min");
                } else if (pair.first->text() == "TTS Max:") {
                    pair.second->setText(QString::number(m_divePlan->m_maxResult.second, 'f', 0) + " min");
                } else if (pair.first->text() == "AP:") {
                    pair.second->setText(QString::number(m_divePlan->m_ap, 'f', 0) + " bar" + 
                        (g_parameters.m_calculateAPandTPonOneTank ? " (on one tank)" : " (on all tanks)"));
                }
            }
        }
    }
    
    // Set mission value if needed - no decimals
    if (!missionEdit->hasFocus()) {
        missionEdit->setText(QString::number(m_divePlan->m_mission, 'f', 0));
    }
    
    // Show/hide mission-related rows
    bool hasMission = (m_divePlan->m_mission > 0);
    
    // Find and update Turn TTS visibility and values
    for (const auto& pair : m_summaryWidgets) {
        if (pair.first->text() == "Turn TTS:") {
            pair.first->setVisible(hasMission);
            pair.second->setVisible(hasMission);
            
            if (hasMission) {
                turnTtsLabel->setText(QString::number(m_divePlan->m_turnTts, 'f', 0) + " min");
            }
            break;
        }
    }
    
    // Show/hide TP row (only in OC mode and with mission set)
    bool showTP = (m_divePlan->m_mode == diveMode::OC && hasMission);
    
    // Find and update TP visibility and values
    for (const auto& pair : m_summaryWidgets) {
        if (pair.first->text() == "TP:") {
            pair.first->setVisible(showTP);
            pair.second->setVisible(showTP);
            
            if (showTP) {
                tpLabel->setText(QString::number(m_divePlan->m_tp, 'f', 0) + " bar" + 
                    (g_parameters.m_calculateAPandTPonOneTank ? " (on one tank)" : " (on all tanks)"));
            }
            break;
        }
    }
    
    // Allow UI to process events after the edit
    QApplication::processEvents();

    // Monitor performance
    logWrite("DivePlanWindow::refreshDiveSummaryTable() took ", timer.elapsed(), " ms");
}

void DivePlanWindow::onGFChanged() {
    // Read the new values
    bool lowOk = false, highOk = false;
    int gfLow = gfLowEdit->text().toInt(&lowOk);
    int gfHigh = gfHighEdit->text().toInt(&highOk);
    
    if (!lowOk || !highOk) {
        // Restore previous values
        gfLowEdit->setText(QString::number(g_parameters.m_gf[0]));
        gfHighEdit->setText(QString::number(g_parameters.m_gf[1]));
        return;
    }
    
    // Apply changes
    g_parameters.m_gf[0] = gfLow;
    g_parameters.m_gf[1] = gfHigh;
    
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::onMissionChanged() {
    // Read the new value
    bool ok = false;
    double mission = missionEdit->text().toDouble(&ok);
    
    if (!ok) {
        // Restore previous value
        missionEdit->setText(QString::number(m_divePlan->m_mission, 'f', 0));
        return;
    }
    
    // Apply change
    m_divePlan->m_mission = mission;
    
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

} // namespace DiveComputer