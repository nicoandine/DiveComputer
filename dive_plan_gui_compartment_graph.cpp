#include "dive_plan_gui_compartment_graph.hpp"

namespace DiveComputer {

CompartmentGraphWindow::CompartmentGraphWindow(const DivePlan* divePlan, QWidget *parent)
    : QMainWindow(parent),
      m_divePlan(divePlan){
    // Set window title with dive number
    setWindowTitle(QString("Compartments for dive %1").arg(m_divePlan->m_diveNumber));
    
    // Configure window size and position
    setWindowSizeAndPosition(this, WindowWidth, WindowHeight, WindowPosition::CENTER);
    
    // Setup UI components
    setupUI();
    
    // Show the first compartment by default
    updateGraph(0);
}

void CompartmentGraphWindow::setupUI(){
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Create toolbar-like widget for the selectors
    QWidget* toolbarWidget = new QWidget(centralWidget);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(10, 5, 10, 5);
    
    // Create gas type selector label
    QLabel* gasTypeLabel = new QLabel("Gas:", toolbarWidget);
    toolbarLayout->addWidget(gasTypeLabel);
    m_graphGasTypeSelector = new QComboBox(toolbarWidget);
    m_graphGasTypeSelector->addItem(returnQStringGasType(GraphGasType::INERT), static_cast<int>(GraphGasType::INERT));
    m_graphGasTypeSelector->addItem(returnQStringGasType(GraphGasType::N2), static_cast<int>(GraphGasType::N2));
    m_graphGasTypeSelector->addItem(returnQStringGasType(GraphGasType::HE), static_cast<int>(GraphGasType::HE));
    connect(m_graphGasTypeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CompartmentGraphWindow::onGasTypeChanged);
    toolbarLayout->addWidget(m_graphGasTypeSelector);
    toolbarLayout->addSpacing(20);
    
    // Create compartment selector label
    QLabel* selectorLabel = new QLabel("Compartment:", toolbarWidget);
    toolbarLayout->addWidget(selectorLabel);
    m_compartmentSelector = new QComboBox(toolbarWidget);
    for (int i = 1; i <= NUM_COMPARTMENTS; ++i) {
        m_compartmentSelector->addItem(QString::number(i), i - 1); // Store 0-indexed value
    }
    connect(m_compartmentSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CompartmentGraphWindow::onCompartmentChanged);
    toolbarLayout->addWidget(m_compartmentSelector);
    toolbarLayout->addSpacing(20);
    
    // Create graph mode selector label
    QLabel* graphModeLabel = new QLabel("Graph against:", toolbarWidget);
    toolbarLayout->addWidget(graphModeLabel);
    m_graphModeSelector = new QComboBox(toolbarWidget);
    m_graphModeSelector->addItem("Ambient Pressure", static_cast<int>(GraphMode::PRESSURE));
    m_graphModeSelector->addItem("Time", static_cast<int>(GraphMode::TIME));
    connect(m_graphModeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CompartmentGraphWindow::onGraphModeChanged);
    toolbarLayout->addWidget(m_graphModeSelector);
    
    // Add spacer to push elements to the left and add toolbar to the main layout
    toolbarLayout->addStretch();
    mainLayout->addWidget(toolbarWidget);
    
    // Create graph widget
    m_graphWidget = new QCustomPlot(centralWidget);
    m_graphWidget->setInteraction(QCP::iRangeDrag, true);
    m_graphWidget->setInteraction(QCP::iRangeZoom, true);
    
    // Setup graph
    setupGraph();
    
    // Add graph to main layout
    mainLayout->addWidget(m_graphWidget, 1); // 1 = stretch factor
}

void CompartmentGraphWindow::setupGraph(){
    // Clear any existing graphs
    m_graphWidget->clearGraphs();
    
    // Create the three graphs we need
    m_graphWidget->addGraph(); // Graph 0: Actual pressure
    m_graphWidget->addGraph(); // Graph 1: Max adjusted pressure
    m_graphWidget->addGraph(); // Graph 2: Reference line or ambient pressure
    
    // Set colors and styles for the graphs
    m_graphWidget->graph(0)->setPen(QPen(QColor(0, 114, 189), 2)); // Blue for actual pressure
    m_graphWidget->graph(1)->setPen(QPen(QColor(217, 83, 25), 2)); // Red for max adjusted pressure
    m_graphWidget->graph(2)->setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine)); // Gray dashed for reference
    
    // Get gas type name for labels
    QString gasName = returnQStringGasType(m_graphGasType);
    
    // Set initial names
    m_graphWidget->graph(0)->setName(QString("Actual %1 Pressure").arg(gasName));
    m_graphWidget->graph(1)->setName(QString("Max GF Adjusted %1 Pressure").arg(gasName));
    m_graphWidget->graph(2)->setName(QString("Ambient %1 Pressure").arg(gasName));

    // Set axis labels for pressure mode
    if (m_graphMode == GraphMode::PRESSURE) {
        m_graphWidget->xAxis->setLabel("Ambient Pressure (bar)");
        m_graphWidget->yAxis->setLabel(QString("%1 Pressure (bar)").arg(gasName));
    } else {
        m_graphWidget->xAxis->setLabel("Time (min)");
        m_graphWidget->yAxis->setLabel("Pressure (bar)");
    }
    
    // Enable legend
    m_graphWidget->legend->setVisible(true);
    m_graphWidget->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    
    // Make sure the right and top axis are shown and connected to the respective left and bottom axes
    m_graphWidget->axisRect()->setupFullAxesBox();
}

void CompartmentGraphWindow::updateGraph(int compartmentIndex) {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Get the appropriate profile
    std::vector<DiveStep> profile = m_divePlan->m_diveProfile;

    // Ensure we have valid data to display
    if (profile.empty()) {
        return;
    }
    
    // IMPORTANT: Clear existing data COMPLETELY
    for (int i = 0; i < m_graphWidget->graphCount(); ++i) {
        m_graphWidget->graph(i)->data()->clear();
    }

    // Get gas type name for labels
    QString gasName = returnQStringGasType(m_graphGasType);
    
    // Initialize with extreme values for finding min/max
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();
    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();

    // Process and add data points. 
    // skip the first 3 stepsto get to max depth
    for (int i = 3; i < (int) profile.size(); i++){
        DiveStep step = profile[i];
 
        // Decide the x axis based on mode and populate the y values
        double x_value = (m_graphMode == GraphMode::PRESSURE) ? step.m_pAmbStartDepth : step.m_runTime;
        double y1_value = getGasPressure(step.m_ppActual[compartmentIndex]);
        double y2_value = getGasPressure(step.m_ppMaxAdjustedGF[compartmentIndex]);
        double y3_value = getAmbientGasPressure(step.m_pAmbStartDepth, step);
            
        // Update the 3 graphs
        m_graphWidget->graph(0)->addData(x_value, y1_value);
        m_graphWidget->graph(1)->addData(x_value, y2_value);
        m_graphWidget->graph(2)->addData(x_value, y3_value);

        // update maximums and minimums for scaling
        x_min = std::min(x_min, x_value);
        x_max = std::max(x_max, x_value);
        y_min = std::min(y_min, std::min(y1_value, std::min(y2_value, y3_value)));
        y_max = std::max(y_max, std::max(y1_value, std::max(y2_value, y3_value)));
    }

    // Set the range
    m_graphWidget->xAxis->setRange(x_min, x_max);
    m_graphWidget->yAxis->setRange(y_min, y_max);
    
    // Update axis labels
    QString x_label = (m_graphMode == GraphMode::PRESSURE) ? "Ambient Pressure (bar)" : "Runtime (min)";
    m_graphWidget->xAxis->setLabel(x_label);
    m_graphWidget->yAxis->setLabel(QString("%1 Pressure (bar)").arg(gasName));
            
    // Refresh the graph
    m_graphWidget->replot();

    // Monitor performance
    logWrite("CompartmentGraphWindow::updateGraph() took ", timer.elapsed(), " ms");
}

void CompartmentGraphWindow::resizeEvent(QResizeEvent* event){
    QMainWindow::resizeEvent(event);
    
    // Force a replot when the window is resized
    if (m_graphWidget) {
        m_graphWidget->replot();
    }
}

void CompartmentGraphWindow::onGasTypeChanged(int index){
    // Get the gas type from the item data
    m_graphGasType = static_cast<GraphGasType>(m_graphGasTypeSelector->itemData(index).toInt());
    
    // Update the graph with the current compartment but new gas type
    int compartmentIndex = m_compartmentSelector->itemData(m_compartmentSelector->currentIndex()).toInt();
    
    // Reset the graph setup with new labels
    setupGraph();
    
    // Update the graph with new data
    updateGraph(compartmentIndex);
}

void CompartmentGraphWindow::onCompartmentChanged(int index){
    // Get the actual compartment index (0-based)
    int compartmentIndex = m_compartmentSelector->itemData(index).toInt();
    
    // Update the graph with the new compartment data
    updateGraph(compartmentIndex);
}

void CompartmentGraphWindow::onGraphModeChanged(int index){
    // Get the graph mode from the item data
    m_graphMode = static_cast<GraphMode>(m_graphModeSelector->itemData(index).toInt());
    
    // Reset the graph setup with new labels
    setupGraph();
    
    // Update the graph with the current compartment but new mode
    int compartmentIndex = m_compartmentSelector->itemData(m_compartmentSelector->currentIndex()).toInt();
    updateGraph(compartmentIndex);
}

double CompartmentGraphWindow::getGasPressure(const CompartmentPP& pp) const {
    switch (m_graphGasType) {
        case GraphGasType::N2:
            return pp.m_pN2;
        case GraphGasType::HE:
            return pp.m_pHe;
        case GraphGasType::INERT:
        default:
            return pp.m_pInert;
    }
}

double CompartmentGraphWindow::getAmbientGasPressure(double pressure, const DiveStep& step) const {
    switch (m_graphGasType) {
        case GraphGasType::N2:
            return pressure * step.m_n2Percent / 100.0;
        case GraphGasType::HE:
            return pressure * step.m_hePercent / 100.0;
        case GraphGasType::INERT:
        default:
            return pressure * (step.m_n2Percent + step.m_hePercent) / 100.0;
    }
}

QString CompartmentGraphWindow::returnQStringGasType(GraphGasType type){
    switch (type) {
            case GraphGasType::N2:
                return "N2";
            case GraphGasType::HE:
                return "He";
            case GraphGasType::INERT:
            default:
                return "N2+He";
        }
}


} // namespace DiveComputer