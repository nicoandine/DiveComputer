// dive_plan_tables.cpp
#include "dive_plan_gui.hpp"
#include "main_gui.hpp"

namespace DiveComputer {


void DivePlanWindow::setupDivePlanTable() {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Set up column headers
    QStringList headers;
    headers << "Phase\n" << "Mode\n" << "Depth Range\n(m)" << "Time\n(min)" << "Run Time\n(min)"
        << "pAmb Max\n(bar)" << "pO2 Max\n(bar)" << "O2\n(%)" << "N2\n(%)" << "He\n(%)"
        << "GF\n(%)" << "GF Surf\n(%)" << "SAC\n(L/min)" << "Amb \n(L/min)" << "Step\n(L)"
        << "Density\n(g/L)" << "END -O2\n(m)" << "END +O2\n(m)" << "CNS\n(%)"
        << "CNS Multi\n(%)" << "OTU\n";
    
    // Configure table
    TableHelper::configureTable(divePlanTable, QAbstractItemView::SelectRows);
    TableHelper::setHeaders(divePlanTable, headers);
    
    // Initialize with fixed width columns
    divePlanTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    // Define column widths - using proportional values that will scale well
    // Values represent relative width proportions
    m_originalColumnWidths = {
        40,   // COL_PHASE - slightly wider for phase names
        40,   // COL_MODE
        100,  // COL_DEPTH_RANGE - needs more space for range format
        60,   // COL_TIME
        60,   // COL_RUN_TIME
        60,   // COL_PAMB_MAX
        44,   // COL_PO2_MAX
        44,   // COL_O2_PERCENT - percentage columns can be narrower
        44,   // COL_N2_PERCENT
        44,   // COL_HE_PERCENT
        44,   // COL_GF
        44,   // COL_GF_SURFACE
        44,   // COL_SAC_RATE
        44,   // COL_AMB_CONSUMPTION
        44,   // COL_STEP_CONSUMPTION
        44,   // COL_GAS_DENSITY
        44,   // COL_END_WO_O2
        44,   // COL_END_W_O2
        44,   // COL_CNS_SINGLE
        44,   // COL_CNS_MULTIPLE
        44    // COL_OTU
    };
    
    // Calculate total width for proportions
    m_totalOriginalWidth = 0;
    for (int width : m_originalColumnWidths) {
        m_totalOriginalWidth += width;
    }
    
    // Mark columns as initialized
    m_columnsInitialized = true;
    
    // Adjust vertical header to fit content better
    divePlanTable->verticalHeader()->setDefaultSectionSize(
        divePlanTable->verticalHeader()->defaultSectionSize() * 1.2);
    
    // Trigger an immediate resize to set proper column widths
    QTimer::singleShot(0, this, &DivePlanWindow::resizeDivePlanTable);
    
    // Also attempt an immediate resize
    resizeDivePlanTable();

    // Hide N2 % column
    divePlanTable->setColumnHidden(COL_N2_PERCENT, true);

    // Monitor performance
    logWrite("DivePlanWindow::setupDivePlanTable() took ", timer.elapsed(), " ms");

    // Update the dive plan
    refreshDivePlanTable();
}

void DivePlanWindow::refreshDivePlanTable() {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Use the TableHelper for safe update
    TableHelper::safeUpdate(divePlanTable, nullptr, nullptr, [this]() {
        // Filter steps to hide certain phases where time = 0
        std::vector<DiveStep> filteredSteps;
        for (int i = 0; i < m_divePlan->nbOfSteps(); ++i) {
            const DiveStep& step = m_divePlan->m_diveProfile[i];
            
            // Keep all steps where time is not 0
            if (step.m_time != 0) {
                filteredSteps.push_back(step);
                continue;
            }
            
            // For time = 0, only keep GAS_SWITCH phase
            if (step.m_time == 0 && step.m_phase == Phase::GAS_SWITCH) {
                filteredSteps.push_back(step);
            }
            // Skip other phases when time = 0 (DESCENDING, STOP, DECO, ASCENDING)
        }
        
        // Set row count based on filtered steps
        divePlanTable->setRowCount(filteredSteps.size());
        
        // Temporarily switch to fixed width mode during updates
        QHeaderView::ResizeMode oldMode = divePlanTable->horizontalHeader()->sectionResizeMode(0);
        divePlanTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        
        // Add filtered dive steps to table
        for (int i = 0; i < (int) filteredSteps.size(); ++i) {
            const DiveStep& step = filteredSteps[i];
            
            // Phase
            divePlanTable->setItem(i, COL_PHASE, 
                TableHelper::createReadOnlyCell(getPhaseString(step.m_phase)));
            
            // Mode
            divePlanTable->setItem(i, COL_MODE, 
                TableHelper::createReadOnlyCell(getStepModeString(step.m_mode)));
            
            // Remaining fields are unchanged
            // Depth range
            QString depthRange = QString::number(step.m_startDepth, 'f', 0) + " → " +
                               QString::number(step.m_endDepth, 'f', 0);
            divePlanTable->setItem(i, COL_DEPTH_RANGE, 
                TableHelper::createReadOnlyCell(depthRange));
            
            // Time
            divePlanTable->setItem(i, COL_TIME,
                TableHelper::createNumericCell(step.m_time, 1, false));
            
            // Run time
            divePlanTable->setItem(i, COL_RUN_TIME, 
                TableHelper::createNumericCell(step.m_runTime, 1, false));
            
            // pAmb Max
            divePlanTable->setItem(i, COL_PAMB_MAX, 
                TableHelper::createNumericCell(step.m_pAmbMax, 2, false));
            
            // pO2 Max
            divePlanTable->setItem(i, COL_PO2_MAX, 
                TableHelper::createNumericCell(step.m_pO2Max, 2, false));
            
            // O2 %
            divePlanTable->setItem(i, COL_O2_PERCENT, 
                TableHelper::createNumericCell(step.m_o2Percent, 0, false));
            
            // N2 %
            divePlanTable->setItem(i, COL_N2_PERCENT, 
                TableHelper::createNumericCell(step.m_n2Percent, 0, false));
            
            // He %
            divePlanTable->setItem(i, COL_HE_PERCENT, 
                TableHelper::createNumericCell(step.m_hePercent, 0, false));
            
            // GF
            divePlanTable->setItem(i, COL_GF, 
                TableHelper::createNumericCell(step.m_gf, 0, false));
            
            // GF Surface
            divePlanTable->setItem(i, COL_GF_SURFACE, 
                TableHelper::createNumericCell(step.m_gfSurface, 0, false));
            
            // SAC Rate
            divePlanTable->setItem(i, COL_SAC_RATE, 
                TableHelper::createNumericCell(step.m_sacRate, 0, false));
            
            // Amb Consumption
            divePlanTable->setItem(i, COL_AMB_CONSUMPTION, 
                TableHelper::createNumericCell(step.m_ambConsumptionAtDepth, 0, false));
            
            // Step Consumption
            divePlanTable->setItem(i, COL_STEP_CONSUMPTION, 
                TableHelper::createNumericCell(step.m_stepConsumption, 0, false));
            
            // Gas Density
            divePlanTable->setItem(i, COL_GAS_DENSITY,
                TableHelper::createNumericCell(step.m_gasDensity, 1, false));
            
            // END without O2
            divePlanTable->setItem(i, COL_END_WO_O2, 
                TableHelper::createNumericCell(step.m_endWithoutO2, 0, false));
            
            // END with O2
            divePlanTable->setItem(i, COL_END_W_O2, 
                TableHelper::createNumericCell(step.m_endWithO2, 0, false));
            
            // CNS Single Dive
            divePlanTable->setItem(i, COL_CNS_SINGLE, 
                TableHelper::createNumericCell(step.m_cnsTotalSingleDive, 0, false));
            
            // CNS Multiple Dives
            divePlanTable->setItem(i, COL_CNS_MULTIPLE, 
                TableHelper::createNumericCell(step.m_cnsTotalMultipleDives, 0, false));
            
            // OTU
            divePlanTable->setItem(i, COL_OTU, 
                TableHelper::createNumericCell(step.m_otuTotal, 0, false));
        }
        
        // Re-enable original resize mode
        divePlanTable->horizontalHeader()->setSectionResizeMode(oldMode);
    });
    
    // Highlight warning cells - this needs to be done after signals are reconnected
    highlightWarningCells();

    // Ensure N2 column remains hidden
    divePlanTable->setColumnHidden(COL_N2_PERCENT, true);

    // Allow UI to process events after the edit
    QApplication::processEvents();

    // Monitor performance
    logWrite("DivePlanWindow::refreshDivePlanTable() took ", timer.elapsed(), " ms");
}

void DivePlanWindow::resizeDivePlanTable() {
    // Skip if not initialized
    if (!divePlanTable || !m_columnsInitialized || m_totalOriginalWidth <= 0) {
        return;
    }
    
    // Force a layout update to ensure we have the correct width
    divePlanTable->updateGeometry();
    divePlanTable->parentWidget()->updateGeometry();
    QApplication::processEvents();
    
    // Calculate the available width (accounting for vertical scrollbar if visible)
    int scrollBarWidth = divePlanTable->verticalScrollBar()->isVisible() ? 
                         divePlanTable->verticalScrollBar()->width() : 0;
    int availableWidth = divePlanTable->width() - scrollBarWidth;
    
    // Temporarily disable updates to prevent flickering
    divePlanTable->setUpdatesEnabled(false);
    
    // Set section resize mode to fixed to allow manual column width adjustment
    divePlanTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    // Count visible columns and calculate visible proportion total
    double totalVisibleProportion = 0.0;
    int visibleColumnCount = 0;
    
    for (int i = 0; i < divePlanTable->horizontalHeader()->count(); ++i) {
        if (!divePlanTable->isColumnHidden(i)) {
            totalVisibleProportion += m_originalColumnWidths[i];
            visibleColumnCount++;
        }
    }
    
    if (visibleColumnCount == 0) {
        divePlanTable->setUpdatesEnabled(true);
        return;
    }
    
    // Calculate proportional widths
    QVector<int> columnWidths(divePlanTable->horizontalHeader()->count(), 0);
    int totalCalculatedWidth = 0;
    
    for (int i = 0; i < divePlanTable->horizontalHeader()->count(); ++i) {
        if (divePlanTable->isColumnHidden(i)) {
            columnWidths[i] = 0;
        } else {
            double proportion = static_cast<double>(m_originalColumnWidths[i]) / totalVisibleProportion;
            int colWidth = static_cast<int>(availableWidth * proportion);
            // Ensure minimum width
            columnWidths[i] = qMax(10, colWidth);
            totalCalculatedWidth += columnWidths[i];
        }
    }
    
    // Handle any rounding errors by adjusting the last visible column
    int lastVisibleCol = divePlanTable->horizontalHeader()->count() - 1;
    while (lastVisibleCol >= 0 && divePlanTable->isColumnHidden(lastVisibleCol)) {
        lastVisibleCol--;
    }
    
    if (lastVisibleCol >= 0) {
        // Adjust the last column to take up any remaining space
        int widthDifference = availableWidth - totalCalculatedWidth;
        columnWidths[lastVisibleCol] = qMax(10, columnWidths[lastVisibleCol] + widthDifference);
    }
    
    // Apply calculated widths
    for (int i = 0; i < divePlanTable->horizontalHeader()->count(); ++i) {
        if (!divePlanTable->isColumnHidden(i)) {
            divePlanTable->setColumnWidth(i, columnWidths[i]);
        }
    }
    
    // Re-enable updates
    divePlanTable->setUpdatesEnabled(true);
}

void DivePlanWindow::highlightWarningCells() {
    // First create a filtered list matching what's displayed in the table
    std::vector<DiveStep> filteredSteps;
    for (int i = 0; i < m_divePlan->nbOfSteps(); ++i) {
        const DiveStep& step = m_divePlan->m_diveProfile[i];
        
        // Keep all steps where time is not 0
        if (step.m_time != 0) {
            filteredSteps.push_back(step);
            continue;
        }
        
        // For time = 0, only keep GAS_SWITCH phase
        if (step.m_time == 0 && step.m_phase == Phase::GAS_SWITCH) {
            filteredSteps.push_back(step);
        }
        // Skip other phases when time = 0 (DESCENDING, STOP, DECO, ASCENDING)
    }
    
    // Now highlight cells based on the filtered steps
    for (int row = 0; row < divePlanTable->rowCount(); ++row) {
        // Use the filtered steps collection instead of directly accessing m_diveProfile
        const DiveStep& step = filteredSteps[row];
        
        // 1. Gas Density
        QTableWidgetItem* gasItem = divePlanTable->item(row, COL_GAS_DENSITY);
        if (gasItem) {
            TableHelper::highlightCell(gasItem, step.m_gasDensity > g_parameters.m_warningGasDensity);
        }
        
        // 2. pO2 Max (too high or too low)
        QTableWidgetItem* pO2Item = divePlanTable->item(row, COL_PO2_MAX);
        if (pO2Item) {
            TableHelper::highlightCell(pO2Item, 
                (step.m_pO2Max > g_parameters.m_PpO2Deco || step.m_pO2Max < g_parameters.m_warningPpO2Low));
        }
        
        // 3. CNS Single Dive
        QTableWidgetItem* cnsItem = divePlanTable->item(row, COL_CNS_SINGLE);
        if (cnsItem) {
            TableHelper::highlightCell(cnsItem, step.m_cnsTotalSingleDive > g_parameters.m_warningCnsMax);
        }
        
        // 4. OTU Total
        QTableWidgetItem* otuItem = divePlanTable->item(row, COL_OTU);
        if (otuItem) {
            TableHelper::highlightCell(otuItem, step.m_otuTotal > g_parameters.m_warningOtuMax);
        }
    }
}


} // namespace DiveComputer