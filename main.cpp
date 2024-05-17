#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <ranges>
#include <algorithm>
#include <fstream>
 
#include "tclap/CmdLine.h"
#include "KZip.hpp"

#include "KmlExport.h"

int main(int argc, char* argv[])
{
	TCLAP::CmdLine cmd("KmlExport", ' ', "1.0.0");
	TCLAP::UnlabeledValueArg<std::string> filename("file", "KML/KMZ file to process", true, "", "string", cmd);
	TCLAP::UnlabeledValueArg<std::string> folderName("folder", "Folder name to dump from the KML file", false, "", "string", cmd);

	cmd.parse(argc, argv);

	try {
		KmlExport kd(filename.getValue());

		auto f = kd.load(folderName.getValue());

		if (!f.placemarks.empty()) {
			auto const outFile = folderName.getValue()+".csv";
			std::fstream out(outFile, std::fstream::out);
			if (!out.is_open()) {
				std::cerr << "Failed to create export file: " << outFile << std::endl;
				return 2;
			}

			std::accumulate(std::next(begin(f.fields)), end(f.fields), *(begin(f.fields)), [](const std::string a, const std::string b) {return a + "," + b;});
			{
				auto const it = cbegin(f.fields);
				out << *it;
				std::ranges::for_each(std::next(it), cend(f.fields), [&out](auto const& p) { out << ',' << p; });
			}
			{
				auto const it = cbegin(f.variableFields);
				out << *it;
				std::ranges::for_each(std::next(it), cend(f.variableFields), [&out](auto const& p) { out << ',' << p; });
			}
			out << std::endl;

			for (auto&& p : f.placemarks) {
				out << p.id << ',' << std::quoted(p.name) << 
					"," << p.coordinates.latitude <<
					"," << p.coordinates.longitude <<
					"," << p.coordinates.elevation << ",";

				for (auto const& variableField : f.variableFields) {
					out << ',';
					if (auto const& it = p.variableData.find(variableField); it != end(p.variableData)) {
						out << std::quoted(it->second);
					}
				}
				out << std::endl;
			}
		}	
	} catch (std::exception const& e) {
		std::cerr << __func__ << ": Exception: " << e.what() << std::endl;
	}

	return 0;
}
