#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <ranges>
#include <algorithm>
#include <fstream>
 
#include "tclap/CmdLine.h"
#include "KZip.hpp"

#include "KmlDumper.h"

int main(int argc, char* argv[])
{
	TCLAP::CmdLine cmd("KmlDumper", ' ', "1.0.0");
	TCLAP::UnlabeledValueArg<std::string> filename("file", "KML/KMZ file to process", true, "", "string", cmd);
	TCLAP::UnlabeledValueArg<std::string> folderName("folder", "Folder name to dump from the KML file", false, "", "string", cmd);

	cmd.parse(argc, argv);

	try {
		KmlDumper kd(filename.getValue());

		kd.test();

		return 0;

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
			out << std::endl;

			for (auto&& p : f.placemarks) {
				out << p.id << "," << p.name <<
					"," << p.coordinates.latitude <<
					"," << p.coordinates.longitude <<
					"," << p.coordinates.elevation << ",";

				{
					auto const it = cbegin(p.details);
					out << it->second;
					std::ranges::for_each(std::next(it), cend(p.details), [&out](auto const& p) { out << ',' << p.second; });
				}

				out << std::endl;
			}
		}	
	} catch (std::exception const& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	return 0;
}
