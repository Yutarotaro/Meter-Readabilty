#include "Eigen/Dense"
#include "common/init.hpp"
#include "external/AdaptiveIntegralThresholding/thresh.hpp"
#include "external/cmdline/cmdline.h"
#include "params/pose_params.hpp"
#include "pos/calib.hpp"
#include "pos/homography.hpp"
#include "read/difference.hpp"
#include "read/readability.hpp"
#include "read/template.hpp"
#include <fstream>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>

Camera_pose camera;


cv::Mat temp;

std::vector<cv::KeyPoint> keypoints1;
cv::Mat descriptors1;

std::vector<cv::KeyPoint> keypointsP;
cv::Mat descriptorsP;

//対応点
std::vector<cv::Point> featurePoint;
std::vector<cv::Point> featurePoint2;

//H:Template to Test Homography
cv::Mat H;
cv::Mat HP;

//Eigen::Matrix<float, 6, 1> param;

//テスト画像のindex
int it;

int meter_type;
std::string meter_type_s;
//type 0: normal, 1: pointer_considered
int type;
//record 0: no, 1:Yes
int record;
int message(int argc, char** argv);

/*class Params
{
public:
    int thresh;          //2値化の閾値
    int total;           //枚数
    std::string picdir;  //ディレクトリ
    cv::Point tl;        //左上
    int l;               //矩形の大きさ
    double front_value;  //正面から読んだときの値
    double front_rad;    //正面から読み取った針の角度
    double k;            //1[deg]に対する変化率
};
*/


//0:T, 1:V
Init::Params params[] = {
    {70, 126, "meter_experiment", cv::Point(1899, 979), 620, 25.2, 1.81970, 80. / CV_PI},
    {100, 92, "meter_experiment_V", cv::Point(1808, 1033), 680, -0.0101, 1.88299, 33.5 * 0.002 / CV_PI},
    {100, 60, "dia_experiment_V", cv::Point(1808, 1033), 680, -0.0098, 1.8666, 33.5 * 0.002 / CV_PI},
};

std::map<std::string, int> mp;

int ite = 1;  //erode, dilate

int main(int argc, char** argv)
{
    //cmdline::parser parser;
    //入力が正しいか確認
    if (message(argc, argv)) {
        std::cout << "unexpected inputs" << std::endl;
        return -1;
    }


    std::string s = "";
    if (argc >= 4) {
        s = "2";
    }

    //parameter
    Init::parseInit();

    //読み取り結果を記録
    std::string fileName = "./diffjust/" + meter_type_s + "/reading/reading.csv";
    std::ofstream ofs(fileName, std::ios::app);


    std::cout << meter_type_s << std::endl;
    cv::Mat Base_clock = cv::imread("../pictures/meter_template/Base_clock" + meter_type_s + ".png", 1);


    //Base_clockの特徴点を保存
    Init::Feature Temp("../pictures/meter_template/temp" + meter_type_s + s + ".png");


    //index of test image
    it = std::stoi(argv[1]);


    std::cout << "picture " << it << std::endl;
    std::string path = "../pictures/" + params[meter_type].picdir + "/pic" + std::to_string(it) + ".JPG";

    cv::Mat Now_clock = cv::imread(path, 1);  //for matching


    //object detection で 切り取られたメータ領域の画像
    cv::Mat init = cv::imread("../pictures/" + params[meter_type].picdir + "/roi/pic" + std::string(argv[1]) + ".png", 1);
    //将来的にinitの方もclass管理したい(名前が衝突するからNowにしたい)
    //Init::Feature Now("../pictures/" + params[meter_type].picdir + "/roi/pic" + std::string(argv[1]) + ".png");


    std::ostringstream ostr;
    ostr << "./minimum.xml";

    //[ref] https://qiita.com/wakaba130/items/3ce8d8668d0a698c7e1b

    cv::FileStorage fs(ostr.str(), cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "File can not be opened." << std::endl;
    }


    //template と roi のスケールを合わせる
    double rate = std::max((double)Temp.img.cols / init.cols, (double)Temp.img.rows / init.rows);
    cv::resize(init, init, cv::Size(), rate, rate);


    cv::Mat edge;
    cv::Mat edge_temp;
    init.copyTo(edge);
    Temp.img.copyTo(edge_temp);
    //11/10ここで一回initを先鋭化しておく
    int unsharp = 1;
    if (unsharp) {
        double k = 3.;
        cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -k, 0,
            -k, 4 * k + 1., -k,
            0, -k, 0);


        cv::filter2D(
            init,
            edge,
            -1,
            kernel,
            cv::Point(-1, -1),
            0,
            cv::BORDER_DEFAULT);
    }

    H = Module::getHomography(Temp.keypoints, Temp.descriptors, edge_temp, edge);
    std::cout << H << std::endl;
    //        return 0;


    //initをtempに重ねて可読性判定
    cv::Mat warped = cv::Mat::zeros(Temp.img.rows, Temp.img.cols, CV_8UC3);
    cv::warpPerspective(init, warped, H.inv(), warped.size());

    cv::imwrite("./diffjust/" + meter_type_s + "/transformed/pic" + std::to_string(it) + (unsharp ? "unsharp" : "") + ".png", warped);

    ////////////////////////////////////////AdaptiveIntegralThresholding

    cv::Mat gray_temp;
    cv::cvtColor(Temp.img, gray_temp, cv::COLOR_BGR2GRAY);
    cv::Mat bwt = cv::Mat::zeros(gray_temp.size(), CV_8UC1);
    Adaptive::thresholdIntegral(gray_temp, bwt);
    cv::dilate(bwt, bwt, cv::Mat(), cv::Point(-1, -1), 1);

    cv::Mat gray_warped;
    cv::cvtColor(warped, gray_warped, cv::COLOR_BGR2GRAY);
    cv::Mat bwi = cv::Mat::zeros(gray_warped.size(), CV_8UC1);
    Adaptive::thresholdIntegral(gray_warped, bwi);
    cv::erode(bwi, bwi, cv::Mat(), cv::Point(-1, -1), 1);


    ////////////////////////////////////////

    cv::imshow("bwi", bwi);
    cv::imshow("bwt", bwt);


    cv::Mat diff = bwi - bwt;


    int d = 90;
    cv::Mat mask_for_dif = cv::Mat::zeros(diff.rows, diff.cols, CV_8UC1);
    cv::circle(mask_for_dif, cv::Point(diff.cols / 2, diff.rows / 2), diff.cols / 2 - d / 2, cv::Scalar(255), -1, 0);

    cv::Mat dif;  //meter領域のみ残した差分画像
    diff.copyTo(dif, mask_for_dif);
    //diff.copyTo(dif);
    ///////////////////
    cv::imshow("dif", dif);
    //cv::waitKey();

    //cv::imwrite("./diffjust/" + meter_type_s + "/diff/" + std::to_string(it) + (type ? "pointer" : "normal") + ".png", dif);
    cv::erode(dif, dif, cv::Mat(), cv::Point(-1, -1), ite);
    cv::dilate(dif, dif, cv::Mat(), cv::Point(-1, -1), ite);


    cv::imshow("diff", dif);
    cv::imwrite("./diffjust/" + meter_type_s + "/diff_min/pic" + std::to_string(it) + (unsharp ? "unsharp" : "") + ".png", dif);


    cv::Mat warped_dif;
    cv::Mat dif_24;
    cv::cvtColor(dif, dif_24, CV_GRAY2BGR);
    warped.copyTo(warped_dif, dif_24);
    cv::imshow("warped_dif", warped_dif);
    //    cv::imwrite("./diffjust/" + meter_type_s + "/diff_min/pic" + std::to_string(it) + ".png", warped_dif);


    cv::Mat thinned_dif;
    cv::ximgproc::thinning(dif, thinned_dif, cv::ximgproc::WMF_EXP);


    std::pair<double, cv::Mat> aa;
    aa.first = 0.;
    aa = Readability::pointerDetection(thinned_dif, dif);

    if (record) {
        ofs << it << ',' << aa.first << ',' << (aa.first ? 1 : 0) << ',' << std::endl;
    }


    std::cout << aa.first << std::endl;
    cv::imwrite("./diffjust/" + meter_type_s + "/reading/" + std::to_string(it) + "min.png", aa.second);

    //    cv::waitKey();


    // optimize Homography
    return 0;
}

int message(int argc, char** argv)
{
    mp["T"] = 0;
    mp["V"] = 1;
    mp["dia_V"] = 2;


    meter_type_s = argv[2];
    //meter_type_s = "T";
    std::cout << "type of analog meter:" << meter_type_s << std::endl;
    meter_type = mp[meter_type_s];


    //std::string tmp = argv[2];
    std::string tmp = "0";
    type = std::stoi(tmp);
    std::cout << "type of homography: " << type << std::endl;


    //tmp = argv[3];
    tmp = "1";
    record = std::stoi(tmp);
    std::cout << "record? :" << (record ? "Yes" : "No") << std::endl;


    std::cout << std::endl
              << "/////////////////////////////" << std::endl;

    return 0;
}
