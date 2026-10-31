#pragma once
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <memory>

#include <opencv2/opencv.hpp>

// Qt（用于稳妥处理中文/非 ASCII 路径）
#include <QString>
#include <QFile>
#include <QByteArray>

// ONNX Runtime
#include <onnxruntime_cxx_api.h>

struct PlantDiseaseResult {
    int   index{};
    std::string name;
    float score{};
};

// 病害详细信息结构
struct DiseaseInfo {
    std::string diagnosis;      // 病害诊断/介绍
    std::string severity_intro; // 病害程度说明
    std::string treatment;      // 防治建议
};

struct PlantDiseasePreprocessConfig {
    enum Mode { Stretch = 0, CenterCrop = 1, Letterbox = 2 } mode = Letterbox;
    bool use_imagenet_norm = true;

    cv::Scalar mean = cv::Scalar(0.485, 0.456, 0.406);
    cv::Scalar std  = cv::Scalar(0.229, 0.224, 0.225);
    bool apply_softmax = true;
};

class PlantDisease {
public:
    PlantDisease(const std::string& onnx_path,
            const std::string& label_path,
            const PlantDiseasePreprocessConfig& cfg = {},
            int input_w = -1, int input_h = -1);

    std::vector<PlantDiseaseResult> infer(const cv::Mat& bgr, int topk = 5);
    std::vector<PlantDiseaseResult> inferFromFile(const QString& path, int topk = 5);

    // 获取病害信息
    std::string getDiseaseName(int index) const;
    std::string getPlantName(int index) const;
    bool isHealthy(int index) const;

    // 获取病害详细信息
    DiseaseInfo getDiseaseInfo(int index) const;
    DiseaseInfo getDiseaseInfoByName(const std::string& name) const;

private:
    // 环境 & 会话
    Ort::Env env_;
    Ort::SessionOptions opts_;
    Ort::Session session_{nullptr};
    Ort::AllocatorWithDefaultOptions allocator_;

    // 配置
    PlantDiseasePreprocessConfig cfg_;
    enum class Layout { NCHW, NHWC } layout_ = Layout::NCHW;

    // 输入/输出名称
    std::string input_name_;
    std::string output_name_;

    // 输入尺寸
    int in_w_ = 224;
    int in_h_ = 224;

    // 标签
    std::vector<std::string> labels_;
    std::unordered_map<std::string, std::string> disease_info_; // index -> disease/plant info
    std::unordered_map<std::string, DiseaseInfo> disease_details_; // 病害详细信息

private:
    // 工具与内部实现
    static std::vector<float> softmax(const std::vector<float>& x);
    void loadLabels(const std::string& label_path);
    void parseDiseaseInfo();
    void loadDiseaseDetails(const std::string& label_path);  // 加载病害详细信息
    void resolveInputInfo();

    cv::Mat imreadQString(const QString& path, int flags = cv::IMREAD_COLOR);

    cv::Mat resizeCenterCrop_(const cv::Mat& img, int W, int H) const;
    cv::Mat resizeLetterbox_(const cv::Mat& img, int W, int H) const;
    cv::Mat preprocess(const cv::Mat& bgr) const;

    std::vector<PlantDiseaseResult> topK(const std::vector<float>& probs, int k) const;
};
