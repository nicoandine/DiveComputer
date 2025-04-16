#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include "qtheaders.hpp"
#include "log_info.hpp"
#include "constants.hpp"
#include "parameters.hpp"
#include "enum.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdarg>  // For variable arguments
#include <vector>   // For the buffer

namespace DiveComputer {
    const std::string PARAMETERS_FILE_NAME = "parameters.dat";
    const std::string GASLIST_FILE_NAME = "gaslist.dat";
    const std::string SETPOINTS_FILE_NAME = "setpoints.dat";
    const std::string LOGO_FILE_NAME = "logo.png";
    const int COLUMN_WIDTH = 215;

    // Style constants for consistent UI
    extern const QString PLAIN_STYLE;
    extern const QString EDITABLE_STYLE;
    
    // Styling function
    void applyEditableCellStyle(QTableWidgetItem* item);

    // File functions
    void   ensureAppInfoSet();
    std::string getFilePath(const std::string& filename);

    // UI Window functions
    void setWindowSizeAndPosition(QWidget* window, int preferredWidth, int preferredHeight, WindowPosition position);

    // Diving-dedicated standard functions
    double getDepthFromPressure(double pressure);
    double getPressureFromDepth(double depth);
    double getOptimalHeContent(double depth, double o2Content);
    double getSchreinerEquation(double p0, double halfTime, double pAmbStartDepth, double pAmbEndDepth, double time, double inertPercent);
    double getGF(double depth, double firstDecoDepth);
    double getDouble(const std::string& prompt);
}


#endif