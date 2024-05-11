#include "KmlDumper.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ranges>
#include <regex>
#include <string_view>
#include <sstream>

#include "html-parser/HTMLDocument.h"
#include "KZip.hpp"

namespace {

constexpr std::array<const char *, 8> node_types{
    "null",  "document", "element", "pcdata",
    "cdata", "comment",  "pi",      "declaration"};

struct simple_walker : pugi::xml_tree_walker {
  virtual bool for_each(pugi::xml_node& node) {
    // std::cout << std::string(depth(), 0x20) << node.name() << std::endl;
    if (std::string_view(node.name()) == "Folder") {
      auto const name = std::string_view(node.child("name").value());
      std::cout << std::string(depth(), 0x20) << node_types[node.type()]
                << ": name='" << node.name() << "', id='" << node.attribute("id").value()
                << (!name.empty() ? " " : "") << name
                << "'\n"; 
    }
    return true;  // continue traversal
  }
};

}  // end namespace

KmlDumper::KmlDumper(const std::string &filename)
	: m_filename(filename)
{}

void KmlDumper::test() {
  if (!parse()) {
    return;
  }
  simple_walker walker;
  m_doc.traverse(walker);
}

void dumpFolder(pugi::xml_node node) {
  std::cerr << "ID: " << node.attribute("id").value() << " name: " << node.child_value("name") << std::endl;
  pugi::xpath_node_set set = node.select_nodes("Folder");
  for (auto it = set.begin(); it != set.end(); ++it) {
    // dumpFolder(it->node());
  }
}

Folder KmlDumper::load(const std::string& folderName) {
  if (!parse()) {
    throw std::runtime_error("failed to load " + m_filename);
  }

  auto const listFolders = folderName.empty();

  Folder f;
  // {
  //   auto dumpNodeSet = [](pugi::xpath_node_set& nodes) {
  //     for (auto it = nodes.begin(); it != nodes.end(); ++it) {
  //       std::cerr << "ID: " << it->node().attribute("id").value() << " name: " << it->node().child_value("name") << std::endl;
  //     }
  //   };

  //   pugi::xpath_node_set folders = m_doc.select_nodes("/kml/Document/Folder");
  //   dumpNodeSet(folders);
  //   pugi::xpath_node_set subfolders = m_doc.select_nodes("/kml/Document/Folder/Folder");
  //   dumpNodeSet(subfolders);
  // }

  // return {};

  // auto dumpFolder = [](pugi::xml_node node) {
  //   for (auto n : node.child("Folder")) {
  //     std::cerr << "ID: " << n.attribute("id").value() << " name: " << n.child_value("name") << std::endl;
  //     if (n.first_child().name() == "Folder") {
  //       dumpFolder(n.first_child());
  //     }
  //   }
  // };

  pugi::xpath_node_set folder = m_doc.select_nodes("/kml/Document/Folder");
  for (auto it = folder.begin(); it != folder.end(); ++it) {
    dumpFolder(it->node());
  }

  auto document = m_doc.child("kml").child("Document");
  // for (auto folder : document.child("Folder")) {
  //   // std::cerr << "ID: " << folder.attribute("id").value() << " name: " << folder.child_value("name") << std::endl;
  //   dumpFolder(folder);
  // }

  return {};

  auto folders = document.child("Folder");

  if (listFolders) {
    std::cerr << "ID: " << folders.attribute("id").value() << " name: " << folders.child_value("name") << std::endl;
  } else {
    f.fields.emplace_back("PlacemarkID");
    f.fields.emplace_back("PlacemarkName");
    f.fields.emplace_back("Latitude");
    f.fields.emplace_back("Longitude");
    f.fields.emplace_back("Elevation");
  }

  std::once_flag loaded;
  //for (auto folder = folders.child("Folder"); folder; folder = folder.next_sibling()) {
  for (auto folder = folders.first_child(); folder; folder = folder.next_sibling()) {
    auto name = std::string(folder.child_value("name"));
    std::stringstream ss;
    ss << "  ID: " << folder.attribute("id").value() << " name: ";
    if (name.find(' ')==name.npos) {
      ss << name;
    } else {
        ss << std::quoted(name);
    }
    std::cerr << ss.str() << std::endl;

    if (!folderName.empty() && name == folderName) {
      f.name = name;

      std::cerr << "Loading folder " << name << std::endl;
      for (auto placemark = folder.child("Placemark"); placemark; placemark = placemark.next_sibling()) {
        Placemark p;
        p.id = placemark.attribute("id").value();
        p.name = placemark.child_value("name");
        p.description = placemark.child_value("description");
        p.details = parseDescription(p.description);
        std::call_once(loaded, [&details = p.details, &fields = f.fields]() {
            std::ranges::for_each(details, [&fields](auto const& field) { fields.emplace_back(field.first); });  
          }
        );
        
        /*
					<Point>
					<coordinates>-98.29152999999997,38.22605000000004,0</coordinates>
					</Point>
        */
        std::string_view coords(placemark.child("Point").child_value("coordinates"));
        //    std::cout << "Coords: " << coords << std::endl;
        size_t last{0};
        {
          auto idx = coords.find(",", last);
          auto const s = std::string(coords.data(), idx);
          // std::cout << "latitude: " << s << std::endl;
          p.coordinates.latitude = std::stof(s);
          last = idx + 1;
          // coords.remove_prefix(idx);
        }
        {
          auto const idx = coords.find(",", last);
          auto const s = std::string(coords.data() + last, idx - 1 - last);
          // std::cout << "longitude: " << s << std::endl;
          p.coordinates.longitude = std::stof(s);
          last = idx + 1;
          // coords.remove_prefix(idx);
        }
        {
          auto const s = std::string(coords.data() + last);
          // std::cout << "elevation: " << s << std::endl;
          p.coordinates.elevation = std::stof(s);
        }

        f.placemarks.emplace_back(p);
      }
      break;
    }
  }

  return f;
}

bool KmlDumper::parse() {
  std::filesystem::path path = m_filename;

  if (path.extension() == ".kmz") {
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
  auto result = m_doc.load_file(m_filename.c_str());
  if (!result) {
    return false;
  }

  return true;
}

std::vector<std::pair<std::string, std::string>> KmlDumper::parseDescription(const std::string& data)
{
   std::regex escaped("(&\\w+;)");
 
	try {
		html_parser::HTMLDocument root(data.data());
		//root.inspect();
		auto trs = root.getElementsByTagName("tr");
    std::vector<std::pair<std::string, std::string>> details;
		for (auto const& tr : trs) {
			if (tr.getChildren().size() == 2) {
				auto const c = tr.getChildren();
        details.emplace_back(std::make_pair(c[0].getDirectTextContent(), std::regex_replace(c[1].getDirectTextContent(), escaped, "")));
			}
		}
    return details;
	} catch (std::exception& e) {
		std::cout << "Exception: " << e.what() << std::endl;
    return {};
	}
}