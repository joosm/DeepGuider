#ifndef __DEEPGUIDER_SIMPLE__
#define __DEEPGUIDER_SIMPLE__

#define VVS_NO_ASSERT
#include "dg_core.hpp"
#include "dg_map_manager.hpp"
#include "dg_localizer.hpp"
#include "dg_road_recog.hpp"
#include "dg_poi_recog.hpp"
#include "dg_vps.hpp"
#include "dg_guidance.hpp"
#include "dg_utils.hpp"
#include <chrono>

using namespace dg;
using namespace std;

class DeepGuider
{
public:
    DeepGuider() {}
    ~DeepGuider();

    bool initialize();
    int run();

protected:
    // configuable parameters
    bool m_recording = false;
    bool m_enable_roadtheta = false;
    bool m_enable_vps = true;
    bool m_enable_poi = false;
    //std::string m_server_ip = "127.0.0.1";        // default: 127.0.0.1 (localhost)
    std::string m_server_ip = "129.254.87.96";      // default: 127.0.0.1 (localhost)

    bool m_threaded_run_python = false;
    std::string m_video_header_name = "dg_test_";
    std::string m_srcdir = "./../src";            // path of deepguider/src (required for python embedding)
    const char* m_map_image_path = "data/NaverMap_ETRI(Satellite)_191127.png";
    const char* m_gps_input = "data/191115_ETRI_asen_fix.csv";
    const char* m_video_input = "data/191115_ETRI.avi";

    // internal api's
    bool initializeMapAndPath(dg::LatLon gps_start, dg::LatLon gps_dest);
    void applyGpsData(dg::LatLon gps_datum, dg::Timestamp ts);
    void drawGuiDisplay(cv::Mat& gui_image);
    bool procRoadTheta();
    bool procVps();
    bool procPoi();

    // shared variables for multi-threading
    cv::Mutex m_cam_mutex;
    cv::Mat m_cam_image;
    dg::Timestamp m_cam_capture_time;
    dg::LatLon m_cam_capture_pos;

    cv::Mutex m_vps_mutex;
    cv::Mat m_vps_image;            // top-1 matched streetview image
    dg::ID m_vps_id;                // top-1 matched streetview id
    double m_vps_confidence;        // top-1 matched confidence(similarity)

    cv::Mutex m_localizer_mutex;
    int m_gps_update_cnt = 0;
    bool m_pose_initialized = false;

    // sub modules
    dg::MapManager m_map_manager;
    dg::SimpleLocalizer m_localizer;
    dg::RoadDirectionRecognizer m_roadtheta;
    dg::POIRecognizer m_poi;
    dg::VPS m_vps;
    dg::GuidanceManager m_guider;

    // guidance icons
    cv::Mat m_icon_forward;
    cv::Mat m_mask_forward;
    cv::Mat m_icon_turn_left;
    cv::Mat m_mask_turn_left;
    cv::Mat m_icon_turn_right;
    cv::Mat m_mask_turn_right;
    cv::Mat m_icon_turn_back;
    cv::Mat m_mask_turn_back;
    void drawGuidance(cv::Mat image, dg::GuidanceManager::Guidance guide, cv::Rect rect);

    // local variables
    cx::VideoWriter m_video;
    cv::Mat m_map_image;
    dg::SimpleRoadPainter m_painter;
    dg::CanvasInfo m_map_info;

    dg::ID id_invalid = 0;
    Polar2 rel_pose_defualt = Polar2(-1, CV_PI);     // default relative pose (invalid)
    double confidence_default = -1.0;
};


DeepGuider::~DeepGuider()
{
    if(m_enable_roadtheta) m_roadtheta.clear();
    if(m_enable_poi) m_poi.clear();
    if(m_enable_vps) m_vps.clear();

    close_python_environment();
}


bool DeepGuider::initialize()
{
    printf("Initialize deepguider system...\n");

    // initialize python
    if (!init_python_environment("python3", "", m_threaded_run_python)) return false;
    printf("\tPython environment initialized!\n");

    // initialize map manager
    m_map_manager.setIP(m_server_ip);
    if (!m_map_manager.initialize()) return false;
    printf("\tMapManager initialized!\n");

    // initialize VPS
    std::string module_path = m_srcdir + "/vps";
    if (m_enable_vps && !m_vps.initialize("vps", module_path.c_str())) return false;
    if (m_enable_vps) printf("\tVPS initialized!\n");

    // initialize POI
    module_path = m_srcdir + "/poi_recog";
    if (m_enable_poi && !m_poi.initialize("poi_recognizer", module_path.c_str())) return false;
    if (m_enable_poi) printf("\tPOI initialized!\n");

    // initialize roadTheta
    module_path = m_srcdir + "/road_recog";
    if (m_enable_roadtheta && !m_roadtheta.initialize("road_direction_recognizer", module_path.c_str())) return false;
    if (m_enable_roadtheta) printf("\tRoadTheta initialized!\n");

    // prepare GUI map
    m_painter.setParamValue("pixel_per_meter", 1.045);
    m_painter.setParamValue("canvas_margin", 0);
    m_painter.setParamValue("canvas_offset", { 344, 293 });
    m_painter.setParamValue("grid_step", 100);
    m_painter.setParamValue("grid_unit_pos", { 120, 10 });
    m_painter.setParamValue("node_radius", 4);
    m_painter.setParamValue("node_font_scale", 0);
    m_painter.setParamValue("node_color", { 255, 50, 255 });
    m_painter.setParamValue("edge_color", { 200, 100, 100 });
    //m_painter.setParamValue("edge_thickness", 2);

    // load icon images
    m_icon_forward = cv::imread("data/forward.png");
    m_icon_turn_left = cv::imread("data/turn_left.png");
    m_icon_turn_right = cv::imread("data/turn_right.png");
    m_icon_turn_back = cv::imread("data/turn_back.png");
    cv::threshold(m_icon_forward, m_mask_forward, 250, 1, cv::THRESH_BINARY_INV);
    cv::threshold(m_icon_turn_left, m_mask_turn_left, 250, 1, cv::THRESH_BINARY_INV);
    cv::threshold(m_icon_turn_right, m_mask_turn_right, 250, 1, cv::THRESH_BINARY_INV);
    cv::threshold(m_icon_turn_back, m_mask_turn_back, 250, 1, cv::THRESH_BINARY_INV);

    // load background map image
    m_map_image = cv::imread(m_map_image_path);
    VVS_CHECK_TRUE(!m_map_image.empty());

    // init video recording
    if (m_recording)
    {
        time_t start_t;
        time(&start_t);
        tm _tm = *localtime(&start_t);
        char szfilename[255];
        strftime(szfilename, 255, "%y%m%d_%H%M%S.avi", &_tm);
        std::string filename = m_video_header_name + szfilename;
        m_video.open(filename, 30);
    }

    // reset interval variables
    m_gps_update_cnt = 0;
    m_pose_initialized = false;
    m_cam_image.release();
    m_cam_capture_time = -1;
    m_vps_image.release();
    m_vps_id = 0;
    m_vps_confidence = 0;

    return true;
}

bool DeepGuider::initializeMapAndPath(dg::LatLon gps_start, dg::LatLon gps_dest)
{
    // generate path to the destination
    dg::Path path;
    VVS_CHECK_TRUE(m_map_manager.getPath(gps_start.lat, gps_start.lon, gps_dest.lat, gps_dest.lon, path));
    dg::ID nid_start = path.pts.front().node_id;
    dg::ID nid_dest = path.pts.back().node_id;
    printf("\tPath generated! start=%zu, dest=%zu\n", nid_start, nid_dest);

    // map generated automatically along with the path
    dg::Map& map = m_map_manager.getMap();    
    dg::Node* node_start = map.findNode(nid_start);
    dg::Node* node_dest = map.findNode(nid_dest);
    VVS_CHECK_TRUE(node_start != nullptr);
    VVS_CHECK_TRUE(node_dest != nullptr);
    printf("\tMap loaded!n");

    // download streetview map
    double lat_center = (node_start->lat + node_dest->lat) / 2;
    double lon_center = (node_start->lon + node_dest->lon) / 2;
    double radius = 1000;
    std::vector<StreetView> sv_list;
    m_map_manager.getStreetView(lat_center, lon_center, radius, sv_list);
    printf("\tStreetView images are downloaded! nViews = %d\n", (int)map.views.size());

    // localizer: set map to localizer
    dg::LatLon ref_node(36.383837659737, 127.367880828442);
    VVS_CHECK_TRUE(m_localizer.setReference(ref_node));
    VVS_CHECK_TRUE(m_localizer.loadMap(map));
    printf("\tLocalizer is updated with new map and path!\n");

    // guidance: init map and path for guidance
    VVS_CHECK_TRUE(m_guider.setPathNMap(path, map));
    VVS_CHECK_TRUE(m_guider.initializeGuides());
    printf("\tGuidance is updated with new map and path!\n");

    // draw map
    dg::RoadMap road_map = m_localizer.getMap();
    VVS_CHECK_TRUE(m_painter.drawMap(m_map_image, road_map));
    m_map_info = m_painter.getCanvasInfo(road_map, m_map_image.size());

    // draw path on the map
    dg::DirectedGraph<dg::Point2ID, double>::Node* node_prev = nullptr;
    for (int idx = 0; idx < (int)path.pts.size(); idx++)
    {
        dg::ID node_id = path.pts[idx].node_id;
        dg::DirectedGraph<dg::Point2ID, double>::Node* node = road_map.findNode(node_id);
        if (node){
            if (node_prev) m_painter.drawEdge(m_map_image, m_map_info, node_prev->data, node->data, 0, cv::Vec3b(200, 0, 0), 2);
            if (node_prev) m_painter.drawNode(m_map_image, m_map_info, node_prev->data, 5, 0, cv::Vec3b(50, 0, 255));
            m_painter.drawNode(m_map_image, m_map_info, node->data, 5, 0, cv::Vec3b(50, 0, 255));
            node_prev = node;
        }
    }
    printf("\tGUI map is updated with new map and path!\n");

    return true;    
}


std::vector<std::pair<double, dg::LatLon>> getExampleGPSData(const char* csv_file = "data/191115_ETRI_asen_fix.csv")
{
    cx::CSVReader csv;
    VVS_CHECK_TRUE(csv.open(csv_file));
    cx::CSVReader::Double2D csv_ext = csv.extDouble2D(1, { 2, 3, 7, 8 }); // Skip the header

    std::vector<std::pair<double, dg::LatLon>> data;
    for (auto row = csv_ext.begin(); row != csv_ext.end(); row++)
    {
        double timestamp = row->at(0) + 1e-9 * row->at(1);
        dg::LatLon ll(row->at(2), row->at(3));
        data.push_back(std::make_pair(timestamp, ll));
    }
    return data;
}


int DeepGuider::run()
{
    printf("Run deepguider system...\n");

    // load gps sensor data (ETRI dataset)
    auto gps_data = getExampleGPSData(m_gps_input);
    VVS_CHECK_TRUE(!gps_data.empty());
    printf("\tSample gps data loaded!\n");

    // load image sensor data (ETRI dataset)
    cv::VideoCapture video_data;
    VVS_CHECK_TRUE(video_data.open(m_video_input));
    double video_time_offset = gps_data.front().first - 0.5, video_time_scale = 1.75; // Calculated from 'bag' files
    double video_resize_scale = 0.4;
    cv::Point video_offset(32, 542);
    double video_time = video_time_scale * video_data.get(cv::VideoCaptureProperties::CAP_PROP_POS_MSEC) / 1000 + video_time_offset;
    printf("\tSample video data loaded!\n");

    // start & goal position
    dg::LatLon gps_start = gps_data.front().second;
    dg::LatLon gps_dest = gps_data.back().second;
    VVS_CHECK_TRUE(initializeMapAndPath(gps_start, gps_dest));
    printf("\tgps_start: lat=%lf, lon=%lf\n", gps_start.lat, gps_start.lon);
    printf("\tgps_dest: lat=%lf, lon=%lf\n", gps_dest.lat, gps_dest.lon);

    // GUI window
    cv::namedWindow("deep_guider");

    // run iteration
    int maxItr = (int)gps_data.size();
    int itr = 0;
    bool is_arrived = false;
    while (!is_arrived && itr < maxItr)
    {
        dg::Timestamp t1 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;

        // gps update
        const dg::LatLon gps_datum = gps_data[itr].second;
        const dg::Timestamp gps_time = gps_data[itr].first;
        applyGpsData(gps_datum, gps_time);
        printf("[GPS] lat=%lf, lon=%lf, ts=%lf\n", gps_datum.lat, gps_datum.lon, gps_time);

        // video capture
        cv::Mat video_image;
        while (video_time <= gps_time)
        {
            video_data >> video_image;
            if (video_image.empty()) break;
            video_time = video_time_scale * video_data.get(cv::VideoCaptureProperties::CAP_PROP_POS_MSEC) / 1000 + video_time_offset;
        }
        m_cam_mutex.lock();
        m_cam_image = video_image;
        m_cam_capture_time = video_time;
        m_cam_capture_pos = gps_datum;
        m_cam_mutex.unlock();

        // process vision modules
        if(m_enable_roadtheta) procRoadTheta();
        if(m_enable_vps) procVps();
        if(m_enable_poi) procPoi();

        // get updated pose & localization confidence
        dg::TopometricPose pose_topo = m_localizer.getPoseTopometric();
        dg::Pose2 pose_metric = m_localizer.getPose();
        dg::LatLon pose_gps = m_localizer.getPoseGPS();
        double pose_confidence = m_localizer.getPoseConfidence();
        printf("[Localizer]\n");
        printf("\ttopo: node=%zu, edge=%d, dist=%lf, ts=%lf\n", pose_topo.node_id, pose_topo.edge_idx, pose_topo.dist, gps_time);
        printf("\tmetr: x=%lf, y=%lf, theta=%lf, ts=%lf\n", pose_metric.x, pose_metric.y, pose_metric.theta, gps_time);
        printf("\tgps : lat=%lf, lon=%lf, ts=%lf\n", pose_gps.lat, pose_gps.lon, gps_time);
        printf("\tconfidence: %lf\n", pose_confidence);

        // Guidance: generate navigation guidance
        dg::GuidanceManager::MoveStatus cur_status;
        dg::GuidanceManager::Guidance cur_guide;
        cur_status = m_guider.applyPose(pose_topo);
        cur_guide = m_guider.getGuidance(cur_status);
        printf("%s\n", cur_guide.msg.c_str());

        // check arrival
        // TODO

        // draw GUI display
        cv::Mat gui_image = m_map_image.clone();
        drawGuiDisplay(gui_image);

        // recording
        if (m_recording) m_video << gui_image;

        cv::imshow("deep_guider", gui_image);
        int key = cv::waitKey(1);
        if (key == cx::KEY_SPACE) key = cv::waitKey(0);
        if (key == cx::KEY_ESC) break;

        dg::Timestamp t2 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
        printf("Iteration: %d (it took %lf seconds)\n", itr, t2 - t1);

        // update iteration
        itr++;
    }    
    if (m_recording) m_video.release();
    cv::destroyWindow("deep_guider");    
    printf("End deepguider system...\n");

    return 0;
}


void DeepGuider::drawGuiDisplay(cv::Mat& image)
{
    // cam image
    m_cam_mutex.lock();
    cv::Mat cam_image = m_cam_image.clone();
    m_cam_mutex.unlock();

    // draw cam image as subwindow on the GUI map image
    cv::Rect video_rect;
    double video_resize_scale = 0.4;
    cv::Mat video_image;
    cv::Point video_offset(32, 542);
    if (!cam_image.empty())
    {
        cv::resize(cam_image, video_image, cv::Size(), video_resize_scale, video_resize_scale);
        video_rect = cv::Rect(video_offset, video_offset + cv::Point(video_image.cols, video_image.rows));
        if (video_rect.br().x < image.cols && video_rect.br().y < image.rows) image(video_rect) = video_image * 1;
    }

    // draw vps result
    if (m_enable_vps)
    {
        // top-1 matched streetview image
        cv::Mat sv_image;
        dg::ID sv_id = 0;
        double sv_confidence = 0;
        m_vps_mutex.lock();
        if(!m_vps_image.empty())
        {
            double fy = (double)video_rect.height / m_vps_image.rows;
            cv::resize(m_vps_image, sv_image, cv::Size(), fy, fy);
            sv_id = m_vps_id;
            sv_confidence = m_vps_confidence;
        }
        m_vps_mutex.unlock();

        if (!sv_image.empty())
        {
            double fy = (double)video_rect.height / sv_image.rows;
            cv::resize(sv_image, sv_image, cv::Size(), fy, fy);
            cv::Point sv_offset = video_offset;
            sv_offset.x = video_rect.x + video_rect.width + 20;
            cv::Rect rect(sv_offset, sv_offset + cv::Point(sv_image.cols, sv_image.rows));
            if (rect.x >= 0 && rect.y >= 0 && rect.br().x < image.cols && rect.br().y < image.rows) image(rect) = sv_image * 1;

            cv::Point msg_offset = sv_offset + cv::Point(10, 30);
            double font_scale = 0.8;
            std::string str_confidence = cv::format("Confidence: %.2lf", sv_confidence);
            cv::putText(image, str_confidence.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 255, 255), 5);
            cv::putText(image, str_confidence.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(255, 0, 0), 2);
            std::string str_id = cv::format("ID: %zu", sv_id);
            msg_offset.y += 30;
            cv::putText(image, str_id.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 255, 255), 5);
            cv::putText(image, str_id.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(255, 0, 0), 2);
        }

        // show gps position of top-1 matched image on the map
        if (sv_id > 0)
        {
            dg::StreetView sv = m_map_manager.getStreetView(sv_id);
            if(sv.id == sv_id)
            {
                dg::Point2 sv_pos = m_localizer.toMetric(dg::LatLon(sv.lat, sv.lon));
                m_painter.drawNode(image, m_map_info, dg::Point2ID(0, sv_pos.x, sv_pos.y), 6, 0, cv::Vec3b(255, 255, 0));
            }
        }
    }

    // draw localization & guidance info
    if (m_pose_initialized)
    {
        // draw robot on the map
        m_localizer_mutex.lock();
        dg::TopometricPose pose_topo = m_localizer.getPoseTopometric();
        dg::LatLon pose_gps = m_localizer.getPoseGPS();
        dg::Pose2 pose_metric = m_localizer.toTopmetric2Metric(pose_topo);
        double pose_confidence = m_localizer.getPoseConfidence();
        m_localizer_mutex.unlock();

        m_painter.drawNode(image, m_map_info, dg::Point2ID(0, pose_metric.x, pose_metric.y), 10, 0, cx::COLOR_YELLOW);
        m_painter.drawNode(image, m_map_info, dg::Point2ID(0, pose_metric.x, pose_metric.y), 8, 0, cx::COLOR_BLUE);

        // draw status message (localization)
        cv::String info_topo = cv::format("Node: %zu, Edge: %d, D: %.3fm", pose_topo.node_id, pose_topo.edge_idx, pose_topo.dist);
        cv::putText(image, info_topo, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 5);
        cv::putText(image, info_topo, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2);
        std::string info_confidence = cv::format("Confidence: %.2lf", pose_confidence);
        cv::putText(image, info_confidence, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 5);
        cv::putText(image, info_confidence, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2);

        printf("[Localizer]\n");
        printf("\ttopo: node=%zu, edge=%d, dist=%lf\n", pose_topo.node_id, pose_topo.edge_idx, pose_topo.dist);
        printf("\tmetr: x=%lf, y=%lf, theta=%lf\n", pose_metric.x, pose_metric.y, pose_metric.theta);
        printf("\tgps : lat=%lf, lon=%lf\n", pose_gps.lat, pose_gps.lon);
        printf("\tconfidence: %lf\n", pose_confidence);

        // Guidance: generate navigation guidance
        dg::GuidanceManager::MoveStatus cur_status;
        dg::GuidanceManager::Guidance cur_guide;
        cur_status = m_guider.applyPose(pose_topo);
        cur_guide = m_guider.getGuidance(cur_status);
        printf("%s\n", cur_guide.msg.c_str());

        // draw guidance output on the video image
        if (!video_image.empty())
        {
            drawGuidance(image, cur_guide, video_rect);
        }
    }
}


void DeepGuider::drawGuidance(cv::Mat image, dg::GuidanceManager::Guidance guide, cv::Rect rect)
{
    int guide_cx = rect.x + rect.width / 2;
    int guide_cy = rect.y + m_icon_forward.rows / 2 + 40;
    cv::Point center_pos(guide_cx, guide_cy);

    std::string dir_msg;
    dg::GuidanceManager::Motion cmd = guide.actions[0].cmd;
    if (cmd == dg::GuidanceManager::Motion::GO_FORWARD || cmd == dg::GuidanceManager::Motion::CROSS_FORWARD || cmd == dg::GuidanceManager::Motion::ENTER_FORWARD || cmd == dg::GuidanceManager::Motion::EXIT_FORWARD)
    {
        cv::Mat& icon = m_icon_forward;
        cv::Mat& mask = m_mask_forward;
        int x1 = center_pos.x - icon.cols / 2;
        int y1 = center_pos.y - icon.rows / 2;
        cv::Rect rect(x1, y1, icon.cols, icon.rows);
        if (rect.x >= 0 && rect.y >= 0 && rect.br().x < image.cols && rect.br().y < image.rows) icon.copyTo(image(rect), mask);
        if (cmd == dg::GuidanceManager::Motion::GO_FORWARD) dir_msg = "[Guide] GO_FORWARD";
        if (cmd == dg::GuidanceManager::Motion::CROSS_FORWARD) dir_msg = "[Guide] CROSS_FORWARD";
        if (cmd == dg::GuidanceManager::Motion::ENTER_FORWARD) dir_msg = "[Guide] ENTER_FORWARD";
        if (cmd == dg::GuidanceManager::Motion::EXIT_FORWARD) dir_msg = "[Guide] EXIT_FORWARD";
    }
    if (cmd == dg::GuidanceManager::Motion::TURN_LEFT || cmd == dg::GuidanceManager::Motion::CROSS_LEFT || cmd == dg::GuidanceManager::Motion::ENTER_LEFT || cmd == dg::GuidanceManager::Motion::EXIT_LEFT)
    {
        cv::Mat& icon = m_icon_turn_left;
        cv::Mat& mask = m_mask_turn_left;
        int x1 = center_pos.x - icon.cols + icon.cols / 6;
        int y1 = center_pos.y - icon.rows / 2;
        cv::Rect rect(x1, y1, icon.cols, icon.rows);
        if (rect.x >= 0 && rect.y >= 0 && rect.br().x < image.cols && rect.br().y < image.rows) icon.copyTo(image(rect), mask);
        if (cmd == dg::GuidanceManager::Motion::TURN_LEFT) dir_msg = "[Guide] TURN_LEFT";
        if (cmd == dg::GuidanceManager::Motion::CROSS_LEFT) dir_msg = "[Guide] CROSS_LEFT";
        if (cmd == dg::GuidanceManager::Motion::ENTER_LEFT) dir_msg = "[Guide] ENTER_LEFT";
        if (cmd == dg::GuidanceManager::Motion::EXIT_LEFT) dir_msg = "[Guide] EXIT_LEFT";
    }
    if (cmd == dg::GuidanceManager::Motion::TURN_RIGHT || cmd == dg::GuidanceManager::Motion::CROSS_RIGHT || cmd == dg::GuidanceManager::Motion::ENTER_RIGHT || cmd == dg::GuidanceManager::Motion::EXIT_RIGHT)
    {
        cv::Mat& icon = m_icon_turn_right;
        cv::Mat& mask = m_mask_turn_right;
        int x1 = center_pos.x - icon.cols / 6;
        int y1 = center_pos.y - icon.rows / 2;
        cv::Rect rect(x1, y1, icon.cols, icon.rows);
        if (rect.x >= 0 && rect.y >= 0 && rect.br().x < image.cols && rect.br().y < image.rows) icon.copyTo(image(rect), mask);
        if (cmd == dg::GuidanceManager::Motion::TURN_RIGHT) dir_msg = "[Guide] TURN_RIGHT";
        if (cmd == dg::GuidanceManager::Motion::CROSS_RIGHT) dir_msg = "[Guide] CROSS_RIGHT";
        if (cmd == dg::GuidanceManager::Motion::ENTER_RIGHT) dir_msg = "[Guide] ENTER_RIGHT";
        if (cmd == dg::GuidanceManager::Motion::EXIT_RIGHT) dir_msg = "[Guide] EXIT_RIGHT";
    }
    if (cmd == dg::GuidanceManager::Motion::TURN_BACK)
    {
        cv::Mat& icon = m_icon_turn_back;
        cv::Mat& mask = m_mask_turn_back;
        int x1 = center_pos.x - icon.cols / 2;
        int y1 = center_pos.y - icon.rows / 2;
        cv::Rect rect(x1, y1, icon.cols, icon.rows);
        if (rect.x >= 0 && rect.y >= 0 && rect.br().x < image.cols && rect.br().y < image.rows) icon.copyTo(image(rect), mask);
        dir_msg = "[Guide] TURN_BACK";
    }

    // show direction message
    cv::Point msg_offset = rect.tl() + cv::Point(10, 30);
    cv::putText(image, dir_msg.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 5);
    cv::putText(image, dir_msg.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2);

    // show distance message
    msg_offset = center_pos + cv::Point(50, 10);
    std::string distance = cv::format("D=%.2lfm", guide.distance_to_remain);
    cv::putText(image, distance.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 5);
    cv::putText(image, distance.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2);

    // show guidance message
    msg_offset = rect.tl() + cv::Point(0, rect.height + 25);
    cv::putText(image, guide.msg.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 4);
    cv::putText(image, guide.msg.c_str(), msg_offset, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 0, 0), 2);
}


void DeepGuider::applyGpsData(dg::LatLon gps_datum, dg::Timestamp ts)
{
    // apply gps to localizer
    VVS_CHECK_TRUE(m_localizer.applyGPS(gps_datum, ts));

    // check pose initialization
    m_gps_update_cnt++;
    if (!m_pose_initialized && m_gps_update_cnt > 10)
    {
        m_pose_initialized = true;
    }

    // draw gps history on the GUI map
    dg::Point2 gps_pt = m_localizer.toMetric(gps_datum);
    m_painter.drawNode(m_map_image, m_map_info, dg::Point2ID(0, gps_pt), 2, 0, cv::Vec3b(0, 255, 0));
}


bool DeepGuider::procRoadTheta()
{
    m_cam_mutex.lock();
    cv::Mat cam_image = m_cam_image.clone();
    dg::Timestamp capture_time = m_cam_capture_time;
    dg::LatLon capture_pos = m_cam_capture_pos;
    m_cam_mutex.unlock();

    if (!cam_image.empty() && m_roadtheta.apply(cam_image, capture_time))
    {
        double angle, confidence;
        m_roadtheta.get(angle, confidence);
        VVS_CHECK_TRUE(m_localizer.applyLocClue(id_invalid, Polar2(-1, angle), capture_time, confidence));
    }

    return true;
}


bool DeepGuider::procVps()
{
    m_cam_mutex.lock();
    cv::Mat cam_image = m_cam_image.clone();
    dg::Timestamp capture_time = m_cam_capture_time;
    dg::LatLon capture_pos = m_cam_capture_pos;
    m_cam_mutex.unlock();

    int N = 3;  // top-3
    double gps_accuracy = 1;   // 0: search radius = 230m ~ 1: search radius = 30m
    if (!cam_image.empty() && m_vps.apply(cam_image, N, capture_pos.lat, capture_pos.lon, gps_accuracy, capture_time, m_server_ip.c_str()))
    {
        std::vector<dg::ID> ids;
        std::vector<dg::Polar2> obs;
        std::vector<double> confs;
        std::vector<VPSResult> streetviews;
        m_vps.get(streetviews);
        printf("[VPS]\n");
        for (int k = 0; k < (int)streetviews.size(); k++)
        {
            if (streetviews[k].id <= 0 || streetviews[k].confidence <= 0) continue;        // invalid data
            ids.push_back(streetviews[k].id);
            obs.push_back(rel_pose_defualt);
            confs.push_back(streetviews[k].confidence);
            printf("\ttop%d: id=%zu, confidence=%lf, ts=%lf\n", k, streetviews[k].id, streetviews[k].confidence, capture_time);
        }

        if(ids.size() > 0)
        {
            m_localizer_mutex.lock();
            VVS_CHECK_TRUE(m_localizer.applyLocClue(ids, obs, capture_time, confs));
            m_localizer_mutex.unlock();

            cv::Mat sv_image;
            if(m_map_manager.getStreetViewImage(ids[0], sv_image, "f") && !sv_image.empty())
            {
                m_vps_mutex.lock();
                m_vps_image = sv_image;
                m_vps_id = ids[0];
                m_vps_confidence = confs[0];
                m_vps_mutex.unlock();
            }
            else
            {
                m_vps_mutex.lock();
                m_vps_image = cv::Mat();
                m_vps_id = ids[0];
                m_vps_confidence = confs[0];
                m_vps_mutex.unlock();
            }
        }
        else
        {
            m_vps_mutex.lock();
            m_vps_image = cv::Mat();
            m_vps_id = 0;
            m_vps_confidence = 0;
            m_vps_mutex.unlock();
        }
    }

    return true;
}


bool DeepGuider::procPoi()
{
    m_cam_mutex.lock();
    cv::Mat cam_image = m_cam_image.clone();
    dg::Timestamp capture_time = m_cam_capture_time;
    dg::LatLon capture_pos = m_cam_capture_pos;
    m_cam_mutex.unlock();

    if (!cam_image.empty() && m_poi.apply(cam_image, capture_time))
    {
        std::vector<dg::ID> ids;
        std::vector<Polar2> obs;
        std::vector<double> confs;
        std::vector<POIResult> pois;
        m_poi.get(pois);
        printf("[POI]\n");
        for (int k = 0; k < (int)pois.size(); k++)
        {
            //dg::ID poi_id = m_map_manager.get_poi(pois[k].label);
            dg::ID poi_id = 0;
            ids.push_back(poi_id);
            obs.push_back(rel_pose_defualt);
            confs.push_back(pois[k].confidence);
            printf("\tpoi%d: x1=%d, y1=%d, x2=%d, y2=%d, label=%s, confidence=%lf, ts=%lf\n", k, pois[k].xmin, pois[k].ymin, pois[k].xmax, pois[k].ymax, pois[k].label.c_str(), pois[k].confidence, capture_time);
        }
        VVS_CHECK_TRUE(m_localizer.applyLocClue(ids, obs, capture_time, confs));
    }

    return true;
}


#endif      // #ifndef __DEEPGUIDER_SIMPLE__


#ifdef DG_TEST_SIMPLE
int main()
{
    DeepGuider deepguider;
    if (!deepguider.initialize()) return -1;
    deepguider.run();

    return 0;
}
#endif      // #ifdef DG_TEST_SIMPLE