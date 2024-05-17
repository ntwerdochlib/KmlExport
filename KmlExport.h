#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include "pugixml.hpp"

struct Coordinates
{
	std::string latitude{"0"};
	std::string longitude{"0"};
	std::string elevation{"0"};
};

struct Placemark
{
	std::string_view id;
	std::string_view name;
	std::string_view description;
	std::unordered_map<std::string, std::string> variableData;
	Coordinates coordinates;
	std::string coordinatesArray;
};

struct Folder
{
	std::vector<std::string> fields;
	std::set<std::string> variableFields;
	std::string name;
	std::vector<Placemark> placemarks;
};

class KmlExport
{
public:
	KmlExport() = default;

	KmlExport(const std::string &filename);

	KmlExport(const KmlExport &) = delete;
	KmlExport(KmlExport &&) = delete;

	~KmlExport() = default;

	KmlExport &operator=(const KmlExport &) = delete;
	KmlExport &operator=(KmlExport &&) = delete;

	Folder load(const std::string& folderName = {});

private:
	bool loadKML();
	bool loadKMZ();
	bool loadFile();

private:
	std::string m_filename;
	pugi::xml_document m_doc;
};
