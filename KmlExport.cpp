#include "KmlExport.h"

#include <array>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex> // std::call_once
#include <ranges>
#include <regex>
#include <string_view>
#include <sstream>

#include "html-parser/HTMLDocument.h"
#include "KZip.hpp"

namespace {

[[nodiscard]]
constexpr bool is_space(char q) noexcept
{
   std::array<char, 6> ws{' ', '\t', '\n', '\v', '\r', '\f'};
   return std::any_of(begin(ws), end(ws), [q](auto p) { return p == q; });
};

std::string trim(std::string_view const str)
{
   auto view = str
         | std::views::drop_while(is_space) | std::views::reverse
         | std::views::drop_while(is_space) | std::views::reverse
         ;
    // std::cout << "[" << std::string(view.begin(), view.end()) << "]" << std::endl;
    return {begin(view), end(view)};
}

}

KmlExport::KmlExport(const std::string &filename)
	: m_filename(filename)
{}

Folder KmlExport::load(const std::string& folderName) {
  if (!loadFile()) {
    throw std::runtime_error("failed to load " + m_filename);
  }

  struct nodeWalker
    : pugi::xml_tree_walker
  {
    Folder f;
    int targetDepth{0};
    bool targetFound{false};
    std::string elementName{"Folder"};
    std::string folderName;
    bool listFolders{true};
    bool finished{false};

    nodeWalker(const std::string& folderName)
      : folderName(folderName)
      , listFolders(folderName.empty())
    {}

    std::unordered_map<std::string, std::string> parseDescription(std::string_view data)
    {
      std::regex escaped("(&\\w+;)");

      try {
        html_parser::HTMLDocument root(data.data());
        auto trs = root.getElementsByTagName("tr");
        std::unordered_map<std::string, std::string> data;
        for (auto const& tr : trs) {
          if (tr.getChildren().size() == 2) {
            auto const c = tr.getChildren();
            auto name = trim(std::regex_replace(c[1].getDirectTextContent(), escaped, ""));
            f.variableFields.emplace(c[0].getDirectTextContent());
            data.emplace(c[0].getDirectTextContent(), name);
          }
        }
        return data;
      } catch (std::exception& e) {
        std::cout << __func__ << ": Exception: " << e.what() << std::endl;
        return {};
      }
    }

    virtual bool for_each(pugi::xml_node& node) {
      if (finished) {
        return true;
      }

      auto const currentElement = std::string_view(node.name());
      if (auto const name = std::string_view(node.child_value("name")); targetDepth < depth() && currentElement == elementName && ((folderName.empty() || folderName == name) || targetFound)) {
        if (!folderName.empty() && folderName == name) {
          targetDepth = depth();
          targetFound = true;
          elementName = "Placemark";
          f.fields.emplace_back("PlacemarkID");
          f.fields.emplace_back("PlacemarkName");
          // f.fields.emplace_back("Coordinates");
          f.fields.emplace_back("Longitude");
          f.fields.emplace_back("Latitude");
          f.fields.emplace_back("Elevation");
          f.name = name;
          std::cerr << "Exporting folder " << name << std::endl;
          return true;
        }

        if (listFolders) {
          std::stringstream ss;
          ss << std::string(depth(), 0x20) << "Folder id='" << node.attribute("id").value() << "'";
          if (!name.empty()) {
            ss << " Name: ";
            if (name.find(' ') == name.npos) {
              ss << name;
            } else {
              ss << std::quoted(name);
            }
          }
          std::cout << ss.str() << std::endl;
          return true;
        }

        if (targetFound) {
          Placemark p;
          p.id = node.attribute("id").value();
          p.name = node.child_value("name");
          p.description = node.child_value("description");
          p.variableData = parseDescription(p.description);
          /*
            <Point>
            <coordinates>-98.29152999999997,38.22605000000004,0</coordinates>
            </Point>
          */
          try {
            auto point = node.child("Point");
            if (!point.empty()) {
              auto coords = std::string_view(point.child_value("coordinates"));
              if (!coords.empty()) {
                // p.coordinates.append("\"").append(coords).append("\"");
                size_t last{0};
                {
                  auto idx = coords.find(",", last);
                  p.coordinates.latitude = std::string(coords.data(), idx);
                  last = idx + 1;
                }
                {
                  auto const idx = coords.find(",", last);
                  p.coordinates.longitude = std::string(coords.data() + last, idx - 1 - last);
                  last = idx + 1;
                }
                {
                  p.coordinates.elevation = std::string(coords.data() + last);
                }
              }
            } else {
              auto coords = node.child("MultiGeometry").child("LineString").child("coordinates");
              if (!coords.empty()) {
                p.variableData.emplace("coordinateSet", trim(std::string_view(coords.child_value())));
              }
            }
          } catch (std::exception const& e) {
            std::cerr << "Placemark exception: " << e.what() << std::endl;
          }

          f.placemarks.emplace_back(p);
        }
      } else if (targetFound && currentElement == "Folder" && targetDepth == depth()) {
        std::cerr << currentElement << " targetDepth: " << targetDepth << " depth: " << depth() << std::endl;
        finished = true;
      }

      return true;  // continue traversal
    }
  };

  try {
    nodeWalker walker(folderName);
    if (!m_doc.traverse(walker)) {
      std::cerr << "Failed traversing KML document" << std::endl;
    }
    return walker.f;
  } catch (std::exception const& e) {
    throw std::runtime_error(std::string(__func__).append(": traverse Exception: ").append(e.what()));
  }
}

bool KmlExport::loadFile()
{
  std::filesystem::path path = m_filename;
  if (path.extension() == ".kmz") {
    return loadKMZ();
  }
  return loadKML();
}

bool KmlExport::loadKML()
{
  auto result = m_doc.load_file(m_filename.c_str());
  if (!result) {
    return false;
  }
  return true;
}

bool KmlExport::loadKMZ()
{
  KZip::ZipArchive arch;
  arch.open(m_filename);
  if (!arch.isOpen()) {
    std::cerr << "Failed loading file: " << m_filename << std::endl;
    return false;
  }
  if (auto const result = m_doc.load_string(arch.entry("doc.kml").getData<std::string>().c_str()); !result) {
    std::cerr << "Failed loading XML data" << std::endl;
    return false;
  }
  return true;
}
