#include "gaslist.hpp"

namespace DiveComputer {

// Define the global GasList instance
GasList g_gasList;

GasList::GasList() {
    // Ensure app info is set before any path operations
    ensureAppInfoSet();
    loadGaslistFromFile();
}

void GasList::addGas(double o2Percent, double hePercent, GasType gasType, GasStatus gasStatus) {
    Gas gas(o2Percent, hePercent, gasType, gasStatus);
    m_gases.push_back(gas);
}

void GasList::editGas(int index, double o2Percent, double hePercent, GasType gasType, GasStatus gasStatus) {
    m_gases[index] = Gas(o2Percent, hePercent, gasType, gasStatus);
}

void GasList::deleteGas(int index) {
    m_gases.erase(m_gases.begin() + index);
}

void GasList::clearGaslist() {
    m_gases.clear();
}

bool GasList::loadGaslistFromFile() {
    const std::string filename = getFilePath(GASLIST_FILE_NAME);
    
    logWrite("Trying to load gas list from: ", filename);
    
    // If file doesn't exist, add default gas and return
    if (!std::filesystem::exists(filename)) {
        ErrorHandler::logError("GasList", "Gas list file not found at " + filename + ". Using default values.", 
                              ErrorSeverity::INFO);
        
        // Since file doesn't exist, let's add a default gas (21% O2)
        if (m_gases.empty()) {
            addGas(g_constants.m_oxygenInAir, 0.0, GasType::BOTTOM, GasStatus::ACTIVE);
        }
        
        // Save the current list to establish the file
        saveGaslistToFile();
        
        return false;
    }
    
    // Try to load gas list from file
    return ErrorHandler::tryFileOperation([&]() {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::ios_base::failure("Failed to open gas list file for reading.");
        }
        
        // Clear the list, determine the number of gases and reserve space for them
        m_gases.clear();
        size_t gasCount = 0;
        file.read(reinterpret_cast<char*>(&gasCount), sizeof(gasCount));
        m_gases.reserve(gasCount);
        
        // Read each gas
        for (size_t i = 0; i < gasCount; ++i) {
            double o2Percent = 0.0;
            double hePercent = 0.0;
            GasType gasType = GasType::BOTTOM;
            GasStatus gasStatus = GasStatus::ACTIVE;
            
            file.read(reinterpret_cast<char*>(&o2Percent), sizeof(o2Percent));
            file.read(reinterpret_cast<char*>(&hePercent), sizeof(hePercent));
            file.read(reinterpret_cast<char*>(&gasType), sizeof(gasType));
            file.read(reinterpret_cast<char*>(&gasStatus), sizeof(gasStatus));
            
            if (file.fail()) {
                throw std::ios_base::failure("Error reading gas data from file");
            }
            
            // Add the gas to our list
            addGas(o2Percent, hePercent, gasType, gasStatus);
        }
        
        file.close();
        ErrorHandler::logError("GasList", "Gas list loaded successfully. Loaded " + 
                             std::to_string(gasCount) + " gases.", ErrorSeverity::INFO);
    }, filename, "Error Loading Gas List");
}

bool GasList::saveGaslistToFile() {
    const std::string filename = getFilePath(GASLIST_FILE_NAME);
    
    ErrorHandler::logError("GasList", "Saving gas list to: " + filename, ErrorSeverity::INFO);
    
    return ErrorHandler::tryFileOperation([&]() {
        // Create directories if they don't exist
        std::filesystem::path filePath(filename);
        std::filesystem::create_directories(filePath.parent_path());
        
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw std::ios_base::failure("Failed to open file for writing: " + filename);
        }
    
        // First write the number of gases
        size_t gasCount = m_gases.size();
        file.write(reinterpret_cast<const char*>(&gasCount), sizeof(gasCount));
        
        if (file.fail()) {
            throw std::ios_base::failure("Error writing gas count to file");
        }
        
        // Then write each gas
        for (const Gas& gas : m_gases) {
            file.write(reinterpret_cast<const char*>(&gas.m_o2Percent), sizeof(gas.m_o2Percent));
            file.write(reinterpret_cast<const char*>(&gas.m_hePercent), sizeof(gas.m_hePercent));
            file.write(reinterpret_cast<const char*>(&gas.m_gasType), sizeof(gas.m_gasType));
            file.write(reinterpret_cast<const char*>(&gas.m_gasStatus), sizeof(gas.m_gasStatus));
            
            if (file.fail()) {
                throw std::ios_base::failure("Error writing gas data to file");
            }
        }
    
        file.close();
        
        // Verify the file was created
        if (!std::filesystem::exists(filename)) {
            throw std::filesystem::filesystem_error(
                "File does not exist after save operation", 
                filename,
                std::error_code());
        }
        
        ErrorHandler::logError("GasList", "Gas list saved successfully to " + filename + 
                             ". File size: " + std::to_string(std::filesystem::file_size(filename)) + 
                             " bytes", ErrorSeverity::INFO);
    }, filename, "Error Saving Gas List");
}

const std::vector<Gas>& GasList::getGases() const {
    return m_gases;
}

} // namespace DiveComputer