#include "dive_plan_gui.hpp"
#include "main_gui.hpp"
#include "dive_plan_gui_compartment_graph.hpp"

namespace DiveComputer {

void DivePlanWindow::setupMenu() {
    // Clear any existing actions
    m_divePlanningMenu->clear();
    
    // Create OC Mode action
    m_ocModeAction = new QAction("OC Mode", this);
    m_ocModeAction->setCheckable(true);
    m_ocModeAction->setChecked(m_divePlan->m_mode == diveMode::OC);
    connect(m_ocModeAction, &QAction::triggered, this, &DivePlanWindow::ocModeActivated);
    m_divePlanningMenu->addAction(m_ocModeAction);

    // Create CC Mode action
    m_ccModeAction = new QAction("CC Mode", this);
    m_ccModeAction->setCheckable(true);
    m_ccModeAction->setChecked(m_divePlan->m_mode == diveMode::CC);
    connect(m_ccModeAction, &QAction::triggered, this, &DivePlanWindow::ccModeActivated);
    m_divePlanningMenu->addAction(m_ccModeAction);
    
    // Create Bailout action (only visible in CC mode)
    m_bailoutAction = new QAction("Bailout", this);
    m_bailoutAction->setCheckable(true);
    m_bailoutAction->setChecked(m_divePlan->m_bailout);
    m_bailoutAction->setVisible(m_divePlan->m_mode == diveMode::CC);
    connect(m_bailoutAction, &QAction::triggered, this, &DivePlanWindow::bailoutActionTriggered);
    m_divePlanningMenu->addAction(m_bailoutAction);
    
    // Add additional actions
    m_divePlanningMenu->addSeparator();
    
    // Create SP Boosted action (only visible in CC mode)
    m_gfBoostedAction = new QAction("SP Boosted", this);
    m_gfBoostedAction->setCheckable(true);
    m_gfBoostedAction->setChecked(m_divePlan->m_boosted);
    m_gfBoostedAction->setVisible(m_divePlan->m_mode == diveMode::CC);
    connect(m_gfBoostedAction, &QAction::triggered, this, &DivePlanWindow::gfBoostedActionTriggered);
    m_divePlanningMenu->addAction(m_gfBoostedAction);
        
    // Max time action
    m_maxTimeAction = new QAction("Max time", this);
    m_maxTimeAction->setVisible(m_divePlan->m_mode == diveMode::OC);
    connect(m_maxTimeAction, &QAction::triggered, this, &DivePlanWindow::setMaxTime);
    m_divePlanningMenu->addAction(m_maxTimeAction);
    
    // Optimise a deco gas action
    m_optimiseDecoGasAction = new QAction("Optimise a deco gas", this);
    m_optimiseDecoGasAction->setVisible(m_divePlan->m_mode == diveMode::OC);
    connect(m_optimiseDecoGasAction, &QAction::triggered, this, &DivePlanWindow::optimiseDecoGas);
    m_divePlanningMenu->addAction(m_optimiseDecoGasAction);

    // Add additional actions
    m_divePlanningMenu->addSeparator();
    
    // Plan consecutive dive
    m_planConsecutiveDiveAction = new QAction("Plan consecutive dive", this);
    m_planConsecutiveDiveAction->setVisible(true);
    connect(m_planConsecutiveDiveAction, &QAction::triggered, this, &DivePlanWindow::planConsecutiveDive);
    m_divePlanningMenu->addAction(m_planConsecutiveDiveAction);

    // Save dive
    m_saveDiveAction = new QAction("Save dive", this);
    m_saveDiveAction->setVisible(true);
    m_saveDiveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_saveDiveAction, &QAction::triggered, this, &DivePlanWindow::saveDivePlan);
    m_divePlanningMenu->addAction(m_saveDiveAction);

    // Graph compartments
    m_graphCompartmentsAction = new QAction("Graph compartments", this);
    m_graphCompartmentsAction->setVisible(true);
    connect(m_graphCompartmentsAction, &QAction::triggered, this, &DivePlanWindow::graphCompartments);
    m_divePlanningMenu->addAction(m_graphCompartmentsAction);
}

void DivePlanWindow::setDivePlanningMenu(QMenu* menu) {
    // Store menu reference
    m_divePlanningMenu = menu;
    
    // Setup menu content if we have a valid menu
    if (m_divePlanningMenu) {
        // Clear existing actions
        m_divePlanningMenu->clear();
        
        // Set up the menu with action items
        setupMenu();
        updateMenuState();
    }
}

void DivePlanWindow::activate() {
    m_mainWindow->activateWindowWithMenu(this);
}

void DivePlanWindow::updateMenuState() {
    if (!m_divePlanningMenu || !m_divePlan) return;
    
    // Update the checked state of the mode actions
    m_ccModeAction->setChecked(m_divePlan->m_mode == diveMode::CC);
    m_ocModeAction->setChecked(m_divePlan->m_mode == diveMode::OC);
    
    // Update visibility and checked state of CC-specific actions
    bool inCCMode = (m_divePlan->m_mode == diveMode::CC);
    
    // Update visibility
    m_bailoutAction->setVisible(inCCMode);
    m_gfBoostedAction->setVisible(inCCMode);
    
    // Update checked states
    if (inCCMode) {
        m_bailoutAction->setChecked(m_divePlan->m_bailout);
        m_gfBoostedAction->setChecked(m_divePlan->m_boosted);
    } else {
        m_bailoutAction->setChecked(false);
        m_gfBoostedAction->setChecked(false);
    }
}

// Menu mode actions
void DivePlanWindow::ccModeActivated() {
    // Set CC mode
    m_divePlan->m_mode = diveMode::CC;
    
    // Update the mode display
    updateMenuState();
    
    // Show setpoints table if in CC mode
    updateSetpointVisibility();
    
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::ocModeActivated() {
    // Set OC mode
    m_divePlan->m_mode = diveMode::OC;
    
    // Update the mode display
    updateMenuState();
    
    // Show setpoints table if in CC mode
    updateSetpointVisibility();
    
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::bailoutActionTriggered() {
    // Toggle bailout mode
    m_divePlan->m_bailout = m_bailoutAction->isChecked();
        
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::gfBoostedActionTriggered() {
    // Toggle SP Boosted mode
    m_divePlan->m_boosted = m_gfBoostedAction->isChecked();

    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::setMaxTime() {
    std::pair<double, double> result = m_divePlan->getMaxTimeAndTTS();

    // find the first stop step
    int firstStopIndex = 0;
    for (int i = 1; i < m_divePlan->nbOfSteps(); i++) {
        if (m_divePlan->m_diveProfile[i].m_phase == Phase::STOP) {
            firstStopIndex = i;
            break;
        }
    }
    m_divePlan->m_diveProfile[firstStopIndex].m_time = result.first;
    
    // Update the Stop Steps table
    for (int i = 0; i < (int) m_divePlan->m_stopSteps.m_stopSteps.size(); i++)
    {
        if(m_divePlan->m_stopSteps.m_stopSteps[i].m_depth == m_divePlan->m_diveProfile[firstStopIndex].m_startDepth){
            m_divePlan->m_stopSteps.m_stopSteps[i].m_time = result.first;
        }
    }
    // Refresh the dive plan
    m_divePlan->calculateDivePlan();
    m_divePlan->calculateGasConsumption();
    m_divePlan->calculateDiveSummary();
    refreshWindow();
}

void DivePlanWindow::optimiseDecoGas() { // PLACEHOLDER
    m_divePlan->optimiseDecoGas();
}

void DivePlanWindow::graphCompartments() {
    // Ensure the dive plan is calculated with time profile
    if (m_divePlan->m_timeProfile.empty()) {
        m_divePlan->calculateDivePlan(false);
        m_divePlan->calculateTimeProfile(false);
    }
    
    // Create the compartment graph window if it doesn't exist
    if (!m_compartmentGraphWindow) {
        m_compartmentGraphWindow = std::make_unique<CompartmentGraphWindow>(m_divePlan.get(), this);
    }
    
    // Show the window
    m_compartmentGraphWindow->show();
    m_compartmentGraphWindow->activateWindow();
    m_compartmentGraphWindow->raise();
}    

void DivePlanWindow::planConsecutiveDive() {
    printf("PLAN NEXT DIVE\n");
}

void DivePlanWindow::saveDivePlan() {
    // Create a file dialog for saving dive plan files
    QString filter = "Dive Plan Files (*.dive);;All Files (*)";
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Dive Plan",
        QDir::homePath() + "/dive_plan.dive",  // Default filename
        filter
    );
    
    // Check if user canceled the dialog
    if (filePath.isEmpty()) {
        return;
    }
    
    // Add .dive extension if not present
    if (!filePath.endsWith(".dive", Qt::CaseInsensitive)) {
        filePath += ".dive";
    }
    
    // Show progress dialog
    showProgressDialog("Saving dive plan...");
    
    // Perform save operation
    bool success = m_divePlan->saveDiveToFile(filePath.toStdString());
    
    // Close progress dialog if it's visible
    if (m_progressDialog && m_progressDialog->isVisible()) {
        m_progressDialog->close();
    }
    
    // Inform user of result
    if (!success) {
        QMessageBox::critical(this, "Error", "Failed to save dive plan to file.");
    } else {
        // Update window title to include filename
        QFileInfo fileInfo(filePath);
        setWindowTitle("Dive Plan - " + fileInfo.fileName());
    }
}

} // namespace DiveComputer