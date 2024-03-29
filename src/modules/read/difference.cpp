#include "../common/init.hpp"
#include "difference.hpp"
#include <opencv2/opencv.hpp>
#include <utility>
#include <vector>

namespace Difference
{

cv::Mat origin = Init::input_images("pointer");

int thre = 160;

std::pair<cv::Point, int> circleDetect(cv::Mat& img)
{
    cv::Mat gray;
    //2値化
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    //平滑化
    cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2, 2);

    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT,
        2, gray.rows / 4, 150, 100, fmin(gray.rows, gray.cols) / 16, fmin(gray.rows, gray.cols) / 2);

    int C_x = gray.rows / 2;
    int C_y = gray.cols / 2;

    double tmp = -1e9;
    int index = -1;
    int radius;
    bool flag = false;

    std::cout << "circle num" << circles.size() << std::endl;


    for (size_t i = 0; i < circles.size(); i++) {
        radius = cvRound(circles[i][2]);
        //暫定: 検出された円のうち最も半径の大きいものをメータとみなす
        //TODO: メータの画像から特徴量抽出のような何か
        int x = cvRound(circles[i][0]);
        int y = cvRound(circles[i][1]);

        if (x - radius <= 0 || x + radius >= gray.cols || y - radius <= 0 || y + radius >= gray.rows) {
            continue;
        }

#if 0
        std::cout << i << std::endl;
        cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        radius = cvRound(circles[i][2]);

        // 円の中心を描画します．
        circle(img, center, 3, cv::Scalar(0, 255, 0), -1, 8, 0);
        // 円を描画します．
        circle(img, center, radius, cv::Scalar(0, 0, 255), 3, 8, 0);

        cv::imshow("circles", img);

        cv::waitKey();
        flag = true;
#else

#endif

        int color_tmp = 0;


        double a = 1;  //下の目的関数の中心からのずれの2乗の係数
        double b = 0;

        //目的関数。これが最も大きいものを選びたい。
        double bijiao = -(C_x - x) * (C_x - x) * a - (C_y - y) * (C_y - y) * a + radius * radius * b;

        if (bijiao > tmp) {
            tmp = bijiao;
            index = i;
        }
    }

    //    index = 0;

    if (flag) {
        index = cv::waitKey(100) + 1;
        std::cout << index << std::endl;
    }

    if (index == -1) {
        std::cout << "cannot find a good circle" << std::endl;
    } else {
        std::cout << index << std::endl;
    }

    //    cv::Point center(cvRound(circles[index][0]), cvRound(circles[index][1]));
    //   radius = cvRound(circles[index][2]);

#if 0
    cv::Point center(cvRound(circles[index][0]), cvRound(circles[index][1]));
    radius = cvRound(circles[index][2]);
#else
    cv::Point center(343, 352);
    radius = cvRound(270);
#endif

    int m = 20;
    int thick = 600 + m;
    int tmp_rad = radius + thick / 2 - m;

    // 円の中心を描画します．
    circle(img, center, 3, cv::Scalar(0, 255, 0), -1, 8, 0);
    // 円を描画します．
    //circle(img, center, radius, cv::Scalar(0, 0, 255), 3, 8, 0);


    circle(img, center, tmp_rad, cv::Scalar(0, 0, 0), thick, 8, 0);

    // cv::Mat result;
    // img.copyTo(img, mask);

    cv::namedWindow("circles", 1);
    cv::imshow("circles", img);
    cv::imwrite("circle.jpg", img);

    cv::waitKey();
    return {center, radius};
}

cv::Mat thresh(cv::Mat image)
{
    cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);

    cv::threshold(image, image, thre /*cv::THRESH_OTSU*/, 255, cv::THRESH_BINARY_INV);
    return image;
}

double dist(cv::Point x, cv::Point y)
{
    return sqrt(pow(x.x - y.x, 2) + pow(y.x - y.y, 2));
}

void binary_search()
{
}

void norm(std::pair<double, double>& x)
{
    double nor = std::sqrt(x.first * x.first + x.second * x.second);


    x.first /= nor;
    x.second /= nor;
}

void Lines(cv::Mat src, std::pair<cv::Point, int> circle, std::pair<double, int>& m)
{
    cv::Mat dst, color_dst;


    cv::Canny(src, dst, 50, 200, 3);
    cv::cvtColor(dst, color_dst, cv::COLOR_GRAY2BGR);

    //int toupiao = 155;  //これより投票数が多いもののみが採用される
    //int toupiao = 125;  //これより投票数が多いもののみが採用される

#if 0
    std::vector<cv::Vec2f> lines;
    cv::HoughLines(dst, lines, 1, CV_PI / 180, toupiao);

    for (size_t i = 0; i < lines.size(); i++) {
        float rho = lines[i][0];
        float theta = lines[i][1];
        double a = std::cos(theta), b = std::sin(theta);
        double x0 = a * rho, y0 = b * rho;
        cv::Point pt1(cvRound(x0 + 1000 * (-b)),
            cvRound(y0 + 1000 * (a)));
        cv::Point pt2(cvRound(x0 - 1000 * (-b)),
            cvRound(y0 - 1000 * (a)));
        cv::line(color_dst, pt1, pt2, cv::Scalar(0, 0, 255), 3, 8);
    }
#else
    //確率ハフ変換による直線検出

    int l = 0;
    int r = 300;

    std::vector<cv::Vec4i> lines;
    int ct = 0;
    while (true) {
        ct++;
        int toupiao = (l + r) / 2;
        cv::HoughLinesP(dst, lines, 1, CV_PI / 180, toupiao, 30, 40);

        //std::cout << ct << toupiao << std::endl;

        if (lines.size() == 2) {
            for (size_t i = 0; i < lines.size(); i++) {
                cv::line(color_dst, cv::Point(lines[i][0], lines[i][1]), cv::Point(lines[i][2], lines[i][3]), cv::Scalar(0, 0, cvRound(255 * (i + 1) / double(lines.size()))), 3, 8);
            }
            std::cout << lines.size() << std::endl;
            std::cout << ct << ' ' << toupiao << std::endl;
            break;
        } else if (lines.size() > 2) {
            l = toupiao;
        } else {
            r = toupiao;
        }

        std::cout << toupiao << std::endl;

        if (r - l == 1) {
            std::cout << "cannot" << std::endl;
            return;
        }
    }
    //2本の直線のベクトルの平均をとってスケールから値に変換
    std::pair<double, double> a[2];

    for (int i = 0; i < 2; i++) {
        auto p = lines[i];
        a[i].first = p[0] - p[2];   //x
        a[i].second = p[1] - p[3];  //y
    }

    norm(a[0]);
    norm(a[1]);


    m.first = std::atan(double(a[0].second + a[1].second) / (a[0].first + a[1].first));
    double n = a[0].second + a[1].second;
    double ny = a[0].first + a[1].first;
    std::cout << n << ' ' << ny << std::endl;
    //x成分の符号
    m.second = (n > 0) - (n < 0);

#endif

    imshow("1", color_dst);

    cv::imwrite("output2.jpg", color_dst);

}  // namespace Difference

Data readMeter(cv::Mat src)
{
    Data ret = {0, 0};

    //TODO:検出円を囲む正方形領域をcv::Rectでトリミング
    std::pair<cv::Point, int> circle = circleDetect(src);


    //メーター部分のトリミング
    cv::Rect roi(cv::Point(circle.first.x - circle.second, circle.first.y - circle.second), cv::Size(2 * circle.second, 2 * circle.second));
    cv::Mat subImg = src(roi);


    cv::imshow("trimming", subImg);
    cv::imwrite("detect.jpg", subImg);

    //cv::Mat sub_thre = thresh(subImg);

    std::pair<double, int> m;
    Lines(subImg, circle, m);
    std::cout << "角度は" << m.first << std::endl;
    std::cout << "符号は" << m.second << std::endl;

    double value;
    int n = m.second;
    double offset = 3.712;
    double min_value = -0.1;
    double max_value = 0;

    if (n > 0) {
        //右半分
        value = 100. + 40. * 2. * m.first / CV_PI;
    } else if (n < 0) {
        //左半分
        value = 20. + 40. * 2. * m.first / CV_PI;
    } else {
        value = -0.05;
    }

    //value += offset;


    std::cout << "value = " << value << std::endl;

    /*
    double dsize = (double)circle.second / ;
    cv::resize(origin, tmpl, dsize, 0, 0, INTER_LINEAR);
*/

    //2値化
    //cv::Mat src_thre = thresh(src);
    //cv::Mat origin_thre = thresh(origin);


    /*保留
    //針なし画像との差分をとる
    cv::Mat diff;
    cv::absdiff(src, origin, diff);
    */


    return ret;
}

}  // namespace Difference
