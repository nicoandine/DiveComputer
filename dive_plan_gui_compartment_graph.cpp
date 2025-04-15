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
    
    // Create toolbar-like widget for the compartment selector
    QWidget* toolbarWidget = new QWidget(centralWidget);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(10, 5, 10, 5);
    
    // Create compartment selector label
    QLabel* selectorLabel = new QLabel("Compartment:", toolbarWidget);
    toolbarLayout->addWidget(selectorLabel);
    
    // Create compartment selector dropdown
    m_compartmentSelector = new QComboBox(toolbarWidget);
    for (int i = 1; i <= NUM_COMPARTMENTS; ++i) {
        m_compartmentSelector->addItem(QString::number(i), i - 1); // Store 0-indexed value
    }
    connect(m_compartmentSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CompartmentGraphWindow::onCompartmentChanged);
    toolbarLayout->addWidget(m_compartmentSelector);
    
    // Add spacer to push elements to the left
    toolbarLayout->addStretch();
    
    // Add toolbar to main layout
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
    m_graphWidget->addGraph(); // Graph 2: y = x reference line
    
    // Set colors and styles for the graphs
    m_graphWidget->graph(0)->setPen(QPen(QColor(0, 114, 189), 2)); // Blue for actual pressure
    m_graphWidget->graph(0)->setName("Actual Pressure");
    
    m_graphWidget->graph(1)->setPen(QPen(QColor(217, 83, 25), 2)); // Red for max adjusted pressure
    m_graphWidget->graph(1)->setName("Max GF Adjusted Pressure");
    
    m_graphWidget->graph(2)->setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine)); // Gray dashed for reference
    m_graphWidget->graph(2)->setName("Reference Line (y = x)");
    
    // Set axis labels
    m_graphWidget->xAxis->setLabel("Ambient Pressure (bar)");
    m_graphWidget->yAxis->setLabel("Compartment Pressure (bar)");
    
    // Enable legend
    m_graphWidget->legend->setVisible(true);
    m_graphWidget->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    
    // Make sure the right and top axis are shown and connected to the respective left and bottom axes
    m_graphWidget->axisRect()->setupFullAxesBox();
}

void CompartmentGraphWindow::updateGraph(int compartmentIndex){
    // std::vector<DiveStep> profile = m_divePlan->m_timeProfile;
    std::vector<DiveStep> profile = m_divePlan->m_diveProfile;

    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Ensure we have valid data to display
    if (profile.empty()) {
        return;
    }
    
    // We'll convert the compartment selector index (0-based) to the actual compartment index
    int k = compartmentIndex;
    
    // Clear existing data
    m_graphWidget->graph(0)->data()->clear();
    m_graphWidget->graph(1)->data()->clear();
    m_graphWidget->graph(2)->data()->clear();
    
    // Find the maximum values for scaling
    double maxPAmb = g_constants.m_atmPressureStp; // Initialize with minimum value
    double maxPressure = 0.0;
    
    for (const auto& step : profile) {
        maxPAmb = std::max(maxPAmb, step.m_pAmbStartDepth);
        maxPressure = std::max(maxPressure, step.m_ppActual[k].m_pInert);
        maxPressure = std::max(maxPressure, step.m_ppMaxAdjustedGF[k].m_pInert);
    }
    
    // Add a margin to the maximum value
    maxPressure *= 1.1;
    
    // Fill graph data
    for (const auto& step : profile) {
        // Graph 0: Actual pressure
        m_graphWidget->graph(0)->addData(step.m_pAmbStartDepth, step.m_ppActual[k].m_pInert);
        
        // Graph 1: Max adjusted pressure with GF
        m_graphWidget->graph(1)->addData(step.m_pAmbStartDepth, step.m_ppMaxAdjustedGF[k].m_pInert);
    }
    
    // Graph 2: y = x reference line (add points at start and end of range)
    double minValue = g_constants.m_atmPressureStp;
    m_graphWidget->graph(2)->addData(minValue, minValue);
    m_graphWidget->graph(2)->addData(maxPAmb, maxPAmb);
    
    // Set the axis ranges
    m_graphWidget->xAxis->setRange(minValue, maxPAmb);
    m_graphWidget->yAxis->setRange(0, maxPressure);
    
    // Refresh the graph
    m_graphWidget->replot();

    // Monitor performance
    printf("CompartmentGraphWindow::updateGraph() took %lld ms\n", timer.elapsed());
}

void CompartmentGraphWindow::resizeEvent(QResizeEvent* event){
    QMainWindow::resizeEvent(event);
    
    // Force a replot when the window is resized
    if (m_graphWidget) {
        m_graphWidget->replot();
    }
}

void CompartmentGraphWindow::onCompartmentChanged(int index){
    // Get the actual compartment index (0-based)
    int compartmentIndex = m_compartmentSelector->itemData(index).toInt();
    
    // Update the graph with the new compartment data
    updateGraph(compartmentIndex);
}

} // namespace DiveComputer