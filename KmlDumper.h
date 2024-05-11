#pragma once

#include <string>
#include <vector>

#include "pugixml.hpp"

struct Coordinates
{
	float latitude;
	float longitude;
	float elevation;
};

struct Placemark
{
	std::string id;
	std::string name;
	std::string description;
	std::vector<std::pair<std::string, std::string>> details;
	Coordinates coordinates;
};

struct Folder
{
	std::vector<std::string> fields;
	std::string name;
	std::vector<Placemark> placemarks;
};

class KmlDumper
{
public:
	KmlDumper() = default;

	KmlDumper(const std::string &filename);

	KmlDumper(const KmlDumper &) = delete;
	KmlDumper(KmlDumper &&) = delete;

	~KmlDumper() = default;

	KmlDumper &operator=(const KmlDumper &) = delete;
	KmlDumper &operator=(KmlDumper &&) = delete;

	Folder load(const std::string& folderName = {});

	void test();

private:
	bool parse();
	std::vector<std::pair<std::string, std::string>> parseDescription(const std::string& data);

private:
	std::string m_filename;
	pugi::xml_document m_doc;
};
