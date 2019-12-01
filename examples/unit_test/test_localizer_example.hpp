#ifndef __TEST_LOCALIZER_EXAMPLE__
#define __TEST_LOCALIZER_EXAMPLE__

#include "vvs.h"
#include "dg_core.hpp"
#include "dg_localizer.hpp"

int generateExampleMap(const char* yml_file = "data/NaverMap_ETRI.yml", dg::ID id_offset = 1000, double pixel_per_meter = 1, const dg::LatLon& first_latlon = dg::LatLon(36.3839003, 127.3653537))
{
    // Read 'yml_file' generated from 'simple_picker'
    cv::FileStorage fs;
    try { if (!fs.open(yml_file, cv::FileStorage::READ)) return -1; }
    catch (cv::Exception & e) { return -1; }

    std::vector<std::vector<dg::Point2>> roads;
    cv::FileNode fn_lines = fs.root()["region_data"];
    if (!fn_lines.empty())
    {
        for (cv::FileNodeIterator fn_line = fn_lines.begin(); fn_line != fn_lines.end(); fn_line++)
        {
            std::vector<dg::Point2> pts;
            (*fn_line)["pts"] >> pts;
            if (!pts.empty()) roads.push_back(pts);
        }
    }
    if (roads.empty()) return -2;

    // Print nodes for example map
    printf("dg::Map map;\n");
    dg::UTMConverter converter;
    converter.setReference(first_latlon);
    const dg::Point2& first_node = roads.front().front();
    for (size_t r_idx = 0; r_idx < roads.size(); r_idx++)
    {
        for (size_t n_idx = 0; n_idx < roads[r_idx].size(); n_idx++)
        {
            dg::ID id = id_offset * (r_idx + 1) + n_idx;
            const dg::Point2& node = roads[r_idx][n_idx];
            dg::LatLon ll = converter.toLatLon(dg::Point2((node.x - first_node.x) / pixel_per_meter, (first_node.y - node.y) / pixel_per_meter));
            printf("map.addNode(dg::NodeInfo(%zd, %.12f, %.12f));\n", id, ll.lat, ll.lon);
        }
    }

    // Print nodes for example map
    for (size_t r_idx = 0; r_idx < roads.size(); r_idx++)
    {
        for (size_t n_idx = 1; n_idx < roads[r_idx].size(); n_idx++)
        {
            dg::ID id = id_offset * (r_idx + 1) + n_idx;
            printf("map.addRoad(%zd, %zd);\n", id - 1, id);
        }
    }

    return 0;
}

dg::Map getExampleMap()
{
    // An example map around ETRI
    // - Note: Geodesic data were roughly generated from 'generateExampleMap()' from manually selected nodes on Naver Map.
    //         The geodesic data were represented based on latitude and longitude of the first node selected by Google Maps. They are not globally accurate, but relatively accurate.
    //         To show the base point, please use the following URL, https://www.google.com/maps/@36.3839003,127.3653537,17z

    dg::Map map;
    map.addNode(dg::NodeInfo(1000, 36.383900299999, 127.365353699998));
    map.addNode(dg::NodeInfo(1001, 36.384065394720, 127.366219813688));
    map.addNode(dg::NodeInfo(1002, 36.384285010577, 127.367118235987));
    map.addNode(dg::NodeInfo(1003, 36.384581612379, 127.367714047117));
    map.addNode(dg::NodeInfo(1004, 36.385125333867, 127.368583410497));
    map.addNode(dg::NodeInfo(1005, 36.385331811615, 127.369181116144));
    map.addNode(dg::NodeInfo(1006, 36.385482658431, 127.370326265278));
    map.addNode(dg::NodeInfo(1007, 36.385709539421, 127.371759696097));
    map.addNode(dg::NodeInfo(1008, 36.385812721482, 127.372716323402));
    map.addNode(dg::NodeInfo(1009, 36.385876695470, 127.374766333677));
    map.addNode(dg::NodeInfo(1010, 36.385874356755, 127.375256921149));
    map.addNode(dg::NodeInfo(1011, 36.385890494250, 127.375780570079));
    map.addNode(dg::NodeInfo(1012, 36.385888757528, 127.376315739613));
    map.addNode(dg::NodeInfo(1013, 36.385659755637, 127.376710702477));
    map.addNode(dg::NodeInfo(1014, 36.384749418954, 127.376718481174));
    map.addNode(dg::NodeInfo(1015, 36.382775545623, 127.376737221723));
    map.addNode(dg::NodeInfo(1016, 36.381441808199, 127.376764948278));
    map.addNode(dg::NodeInfo(1017, 36.379062708225, 127.376814401930));
    map.addRoad(1000, 1001);
    map.addRoad(1001, 1002);
    map.addRoad(1002, 1003);
    map.addRoad(1003, 1004);
    map.addRoad(1004, 1005);
    map.addRoad(1005, 1006);
    map.addRoad(1006, 1007);
    map.addRoad(1007, 1008);
    map.addRoad(1008, 1009);
    map.addRoad(1009, 1010);
    map.addRoad(1010, 1011);
    map.addRoad(1011, 1012);
    map.addRoad(1012, 1013);
    map.addRoad(1013, 1014);
    map.addRoad(1014, 1015);
    map.addRoad(1015, 1016);
    map.addRoad(1016, 1017);
    return map;
}

int testLocMap2SimpleRoadMap(int wait_msec = 1, const char* background_file = "data/NaverMap_ETRI(Satellite)_191127.png")
{
    // Load a map
    dg::Map map = getExampleMap();

    // Convert it to 'dg::SimpleRoadMap'
    dg::UTMConverter converter;
    dg::Map::NodeItrConst refer_node = map.getHeadNodeConst(); // Select the first node as the origin
    VVS_CHECK_TRUE(converter.setReference(refer_node->data));
    dg::SimpleRoadMap simple_map = dg::SimpleMetricLocalizer::cvtMap2SimpleRoadMap(map, converter);
    VVS_CHECK_TRUE(!simple_map.isEmpty());

    // Draw the converted map
    dg::SimpleRoadPainter painter;
    VVS_CHECK_TRUE(painter.setParamValue("pixel_per_meter", 1));
    VVS_CHECK_TRUE(painter.setParamValue("canvas_margin", 0));
    VVS_CHECK_TRUE(painter.setParamValue("canvas_offset", { 323, 300 }));
    VVS_CHECK_TRUE(painter.setParamValue("grid_step", 100));
    VVS_CHECK_TRUE(painter.setParamValue("grid_unit_pos", { 120, 10 }));
    VVS_CHECK_TRUE(painter.setParamValue("node_radius", 5));
    VVS_CHECK_TRUE(painter.setParamValue("node_color", { 255, 0, 255 }));
    VVS_CHECK_TRUE(painter.setParamValue("edge_color", { 100, 0, 0 }));

    cv::Mat image = cv::imread(background_file);
    VVS_CHECK_TRUE(painter.drawMap(image, simple_map));
    VVS_CHECK_TRUE(image.empty() == false);

    if (wait_msec >= 0)
    {
        cv::imshow("testLocMap2SimpleRoadMap", image);
        cv::waitKey(wait_msec);
    }

    return 0;
}

#endif // End of '__TEST_LOCALIZER_EXAMPLE__'
