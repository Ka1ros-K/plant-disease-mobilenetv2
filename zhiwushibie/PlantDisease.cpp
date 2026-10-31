#include "PlantDisease.h"
#include <algorithm>
#include <numeric>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QDir>

// ---------- helpers ----------
static inline std::wstring Utf8ToWString_viaQt(const std::string& s) {
    return QString::fromUtf8(s.c_str()).toStdWString();
}

// softmax
std::vector<float> PlantDisease::softmax(const std::vector<float>& x) {
    if (x.empty()) return {};
    float m = *std::max_element(x.begin(), x.end());
    std::vector<float> ex(x.size());
    float sum = 0.f;
    for (size_t i = 0; i < x.size(); ++i) { ex[i] = std::exp(x[i] - m); sum += ex[i]; }
    if (sum == 0.f) return std::vector<float>(x.size(), 0.f);
    for (auto& v : ex) v /= sum;
    return ex;
}

// ---------- ctor ----------
PlantDisease::PlantDisease(const std::string& onnx_path,
                 const std::string& label_path,
                 const PlantDiseasePreprocessConfig& cfg,
                 int input_w, int input_h)
    : env_(ORT_LOGGING_LEVEL_WARNING, "plantdisease"), cfg_(cfg)
{
    qDebug() << "[PlantDisease] 构造开始";
    qDebug() << "[PlantDisease] onnx_path =" << QString::fromUtf8(onnx_path.c_str());
    qDebug() << "[PlantDisease] label_path =" << QString::fromUtf8(label_path.c_str());

    QElapsedTimer tInit; tInit.start();

    // 基本优化设置
    opts_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try {
#ifdef _WIN32
        std::wstring wpath = Utf8ToWString_viaQt(onnx_path);
        session_ = Ort::Session(env_, wpath.c_str(), opts_);
#else
        session_ = Ort::Session(env_, onnx_path.c_str(), opts_);
#endif
        qDebug() << "[PlantDisease] ONNX Session 创建成功。用时 (ms):" << tInit.elapsed();
    } catch (const std::exception& e) {
        qWarning() << "[PlantDisease][ERROR] 创建 Session 失败:" << e.what();
        throw;
    }

    try {
        loadLabels(label_path);
        parseDiseaseInfo();
        loadDiseaseDetails(label_path);  // 加载病害详细信息
        qDebug() << "[PlantDisease] 标签加载完成，数量 =" << int(labels_.size());
    } catch (const std::exception& e) {
        qWarning() << "[PlantDisease][ERROR] 加载标签失败:" << e.what();
        throw;
    }

    if (input_w > 0 && input_h > 0) {
        in_w_ = input_w;
        in_h_ = input_h;
        qDebug() << "[PlantDisease] 指定输入尺寸:" << in_w_ << "x" << in_h_;
    }

    try {
        resolveInputInfo();
        qDebug() << "[PlantDisease] resolveInputInfo 完成。layout="
                 << (layout_ == Layout::NCHW ? "NCHW" : "NHWC")
                 << "in_w=" << in_w_ << "in_h=" << in_h_;
    } catch (const std::exception& e) {
        qWarning() << "[PlantDisease][ERROR] 解析输入信息失败:" << e.what();
        throw;
    }

    qDebug() << "[PlantDisease] 构造完成。总用时 (ms):" << tInit.elapsed();
}

// ---------- labels ----------
void PlantDisease::loadLabels(const std::string& label_path) {
    qDebug() << "[PlantDisease] 开始加载标签:" << QString::fromUtf8(label_path.c_str());

    QFile f(QString::fromUtf8(label_path.c_str()));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Cannot open label file: " + label_path);
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        throw std::runtime_error(("JSON parse error: " + err.errorString()).toStdString());
    }
    if (!doc.isObject()) {
        throw std::runtime_error("Label file root is not a JSON object");
    }

    const QJsonObject root = doc.object();
    if (!root.contains("species_taxons") || !root["species_taxons"].isObject()) {
        throw std::runtime_error("Label file does not contain 'species_taxons' object");
    }
    const QJsonObject taxons = root["species_taxons"].toObject();

    struct Item { int idx; QString name; };
    std::vector<Item> items; items.reserve(taxons.size());

    for (auto it = taxons.begin(); it != taxons.end(); ++it) {
        bool ok = false;
        const int idx = it.key().toInt(&ok);
        if (!ok || !it.value().isObject()) continue;

        const QJsonObject obj = it.value().toObject();
        const QString name = obj.value("chinese_name").toString();
        items.push_back({ idx, name });
    }

    // 按数值键排序，保证与模型类别索引一致
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){ return a.idx < b.idx; });

    labels_.clear();
    labels_.reserve(items.size());

    for (const auto& it : items) {
        labels_.push_back(it.name.toStdString());
    }

    qDebug() << "[PlantDisease] 标签数量:" << int(labels_.size());
}

// ---------- parse disease info ----------
void PlantDisease::parseDiseaseInfo() {
    qDebug() << "[PlantDisease] 解析病害信息...";
    // 标签格式： "Apple___Apple_scab" 或 "Tomato___healthy"
    // 解析出植物名和病害名
    for (size_t i = 0; i < labels_.size(); ++i) {
        const std::string& label = labels_[i];
        size_t sep = label.find("___");
        if (sep != std::string::npos) {
            std::string plant = label.substr(0, sep);
            std::string disease = label.substr(sep + 3);
            disease_info_[std::to_string(i)] = plant + "___" + disease;
        } else {
            disease_info_[std::to_string(i)] = label;
        }
    }
}

// ---------- load disease details ----------
void PlantDisease::loadDiseaseDetails(const std::string& label_path) {
    qDebug() << "[PlantDisease] 加载病害详细信息...";

    // 尝试加载病害详细信息 JSON 文件
    QString labelPath = QString::fromUtf8(label_path.c_str());
    QFileInfo fi(labelPath);
    QString dirPath = fi.absoluteDir().absolutePath();
    QString detailPath = dirPath + "/plant_disease_info.json";

    qDebug() << "[PlantDisease] 尝试加载:" << detailPath;
    qDebug() << "[PlantDisease] 文件存在:" << QFile::exists(detailPath);

    QFile f(detailPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[PlantDisease] 无法打开病害详细信息文件，将使用默认分析";
        return;
    }

    const QByteArray data = f.readAll();
    f.close();

    qDebug() << "[PlantDisease] 读取数据大小:" << data.size() << "bytes";
    qDebug() << "[PlantDisease] 数据前 50 字节:" << data.left(50);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[PlantDisease] JSON 解析错误:" << err.errorString();
        qWarning() << "[PlantDisease] 错误偏移:" << err.offset;
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "[PlantDisease] JSON 根节点不是对象";
        return;
    }

    const QJsonObject root = doc.object();
    if (!root.contains("disease_info") || !root["disease_info"].isObject()) {
        qWarning() << "[PlantDisease] JSON 不包含 disease_info";
        return;
    }

    const QJsonObject diseaseInfo = root["disease_info"].toObject();

    for (auto it = diseaseInfo.begin(); it != diseaseInfo.end(); ++it) {
        const QString key = it.key();
        const QJsonObject obj = it.value().toObject();

        DiseaseInfo info;
        info.diagnosis = obj.value("diagnosis").toString().toStdString();
        info.severity_intro = obj.value("severity_intro").toString().toStdString();
        info.treatment = obj.value("treatment").toString().toStdString();

        disease_details_[key.toStdString()] = info;
    }

    qDebug() << "[PlantDisease] 病害详细信息加载完成，数量:" << disease_details_.size();
}

// ---------- helper methods ----------
std::string PlantDisease::getDiseaseName(int index) const {
    if (index < 0 || index >= static_cast<int>(labels_.size())) return "";
    const std::string& label = labels_[index];
    size_t sep = label.find("___");
    if (sep != std::string::npos) {
        return label.substr(sep + 3);
    }
    return label;
}

std::string PlantDisease::getPlantName(int index) const {
    if (index < 0 || index >= static_cast<int>(labels_.size())) return "";
    const std::string& label = labels_[index];
    size_t sep = label.find("___");
    if (sep != std::string::npos) {
        return label.substr(0, sep);
    }
    return label;
}

bool PlantDisease::isHealthy(int index) const {
    std::string disease = getDiseaseName(index);
    return disease == "healthy";
}

// ---------- get disease info ----------
DiseaseInfo PlantDisease::getDiseaseInfo(int index) const {
    if (index < 0 || index >= static_cast<int>(labels_.size())) {
        return DiseaseInfo{};
    }
    const std::string& label = labels_[index];
    return getDiseaseInfoByName(label);
}

DiseaseInfo PlantDisease::getDiseaseInfoByName(const std::string& name) const {
    auto it = disease_details_.find(name);
    if (it != disease_details_.end()) {
        return it->second;
    }

    // 如果没有找到详细信息，返回默认信息
    DiseaseInfo defaultInfo;
    defaultInfo.diagnosis = "暂无详细病害信息";
    defaultInfo.severity_intro = "根据置信度判断：≥80% 为重度，50-80% 为中度，<50% 为轻度";
    defaultInfo.treatment = "请咨询当地农业技术部门获取专业防治建议";
    return defaultInfo;
}

// ---------- io meta ----------
void PlantDisease::resolveInputInfo() {
    qDebug() << "[PlantDisease] 解析模型 IO 信息...";
    auto in_name  = session_.GetInputNameAllocated(0, allocator_);
    auto out_name = session_.GetOutputNameAllocated(0, allocator_);
    input_name_  = in_name.get();
    output_name_ = out_name.get();
    qDebug() << "[PlantDisease] Input name ="  << QString::fromUtf8(input_name_.c_str());
    qDebug() << "[PlantDisease] Output name =" << QString::fromUtf8(output_name_.c_str());

    auto in_type = session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
    auto in_shape = in_type.GetShape();
    QString shapeStr = "[";
    for (size_t i = 0; i < in_shape.size(); ++i) {
        shapeStr += QString::number(in_shape[i]);
        if (i + 1 < in_shape.size()) shapeStr += ", ";
    }
    shapeStr += "]";
    qDebug() << "[PlantDisease] 原始输入维度 =" << shapeStr;

    // 推断布局 NCHW / NHWC，并更新输入尺寸
    if (in_shape.size() == 4) {
        const int64_t n = in_shape[0];
        const int64_t a = in_shape[1];
        const int64_t b = in_shape[2];
        const int64_t c = in_shape[3];

        if (a == 3 && b > 0 && c > 0) {
            layout_ = Layout::NCHW;
            in_h_ = static_cast<int>(b);
            in_w_ = static_cast<int>(c);
        } else if (c == 3 && a > 0 && b > 0) {
            layout_ = Layout::NHWC;
            in_h_ = static_cast<int>(a);
            in_w_ = static_cast<int>(b);
        } else {
            if (in_h_ <= 0 || in_w_ <= 0) {
                in_h_ = 224; in_w_ = 224;
                qDebug() << "[PlantDisease] 动态维度或无法判断布局，使用默认 224x224";
            }
        }
        Q_UNUSED(n);
    } else {
        qWarning() << "[PlantDisease] 输入不是 4 维张量，预处理将使用默认尺寸:"
                   << in_w_ << "x" << in_h_;
    }
}

// ---------- robust imread (中文/特殊字符路径) ----------
cv::Mat PlantDisease::imreadQString(const QString& path, int flags) {
    qDebug() << "[PlantDisease] imreadQString 读取:" << path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[PlantDisease][ERROR] 无法打开文件:" << path;
        return {};
    }
    QByteArray bytes = f.readAll();
    std::vector<uchar> buf(bytes.begin(), bytes.end());
    auto img = cv::imdecode(buf, flags);
    if (img.empty()) {
        qWarning() << "[PlantDisease][ERROR] cv::imdecode 失败:" << path;
    } else {
        qDebug() << "[PlantDisease] 读取成功，尺寸:" << img.cols << "x" << img.rows << " ch=" << img.channels();
    }
    return img;
}

// ---------- preprocess ----------
cv::Mat PlantDisease::resizeCenterCrop_(const cv::Mat& img, int W, int H) const {
    float scale = std::max(W / (float)img.cols, H / (float)img.rows);
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(), scale, scale, cv::INTER_AREA);
    int x = std::max(0, (resized.cols - W) / 2);
    int y = std::max(0, (resized.rows - H) / 2);
    cv::Rect roi(x, y, std::min(W, resized.cols - x), std::min(H, resized.rows - y));
    cv::Mat out(H, W, img.type());
    out.setTo(cv::Scalar::all(0));
    resized(roi).copyTo(out(cv::Rect((W - roi.width)/2, (H - roi.height)/2, roi.width, roi.height)));
    return out;
}

cv::Mat PlantDisease::resizeLetterbox_(const cv::Mat& img, int W, int H) const {
    float scale = std::min(W / (float)img.cols, H / (float)img.rows);
    int nw = std::max(1, int(std::round(img.cols * scale)));
    int nh = std::max(1, int(std::round(img.rows * scale)));
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);

    cv::Scalar padRgb = cv::Scalar(0, 0, 0);
    if (cfg_.use_imagenet_norm) {
        padRgb = cv::Scalar(cfg_.mean[0]*255.0, cfg_.mean[1]*255.0, cfg_.mean[2]*255.0);
    }
    cv::Mat out(H, W, img.type(), padRgb);
    int x = (W - nw) / 2;
    int y = (H - nh) / 2;
    resized.copyTo(out(cv::Rect(x, y, nw, nh)));
    return out;
}

cv::Mat PlantDisease::preprocess(const cv::Mat& bgr) const {
    if (bgr.empty()) throw std::runtime_error("Input image is empty");
    QElapsedTimer t; t.start();
    qDebug() << "[PlantDisease] 预处理开始，输入尺寸:" << bgr.cols << "x" << bgr.rows;

    cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    switch (cfg_.mode) {
    case PlantDiseasePreprocessConfig::Stretch:
        cv::resize(rgb, resized, cv::Size(in_w_, in_h_), 0, 0, cv::INTER_AREA);
        qDebug() << "[PlantDisease] 预处理模式：Stretch";
        break;
    case PlantDiseasePreprocessConfig::CenterCrop:
        resized = resizeCenterCrop_(rgb, in_w_, in_h_);
        qDebug() << "[PlantDisease] 预处理模式：CenterCrop";
        break;
    case PlantDiseasePreprocessConfig::Letterbox:
        resized = resizeLetterbox_(rgb, in_w_, in_h_);
        qDebug() << "[PlantDisease] 预处理模式：Letterbox";
        break;
    default:
        throw std::runtime_error("Unknown preprocess mode");
    }

    resized.convertTo(resized, CV_32F, 1.0/255.0);

    if (cfg_.use_imagenet_norm) {
        cv::Mat m(resized.size(), CV_32FC3, cfg_.mean);
        cv::Mat s(resized.size(), CV_32FC3, cfg_.std);
        resized = (resized - m) / s;
    }

    qDebug() << "[PlantDisease] 预处理完成，输出尺寸:" << resized.cols << "x" << resized.rows
             << " 用时 (ms):" << t.elapsed();
    return resized;
}

// ---------- topK ----------
std::vector<PlantDiseaseResult> PlantDisease::topK(const std::vector<float>& probs, int k) const {
    if (probs.empty() || k <= 0) {
        qWarning() << "[PlantDisease] topK 输入为空或 k 非法，k=" << k;
        return {};
    }
    k = std::min<int>(k, static_cast<int>(probs.size()));
    std::vector<int> idx(probs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin()+k, idx.end(),
                      [&](int a, int b){ return probs[a] > probs[b]; });

    std::vector<PlantDiseaseResult> out;
    out.reserve(k);
    for (int i = 0; i < k; ++i) {
        int id = idx[i];
        PlantDiseaseResult r;
        r.index = id;
        r.name  = (id >= 0 && id < static_cast<int>(labels_.size())) ? labels_[id] : std::to_string(id);
        r.score = probs[id];
        out.push_back(std::move(r));
    }
    qDebug() << "[PlantDisease] topK 完成，k=" << k;
    return out;
}

// ---------- infer ----------
std::vector<PlantDiseaseResult> PlantDisease::infer(const cv::Mat& bgr, int topk) {
    qDebug() << "[PlantDisease] infer 开始，topk=" << topk
             << " layout=" << (layout_ == Layout::NCHW ? "NCHW" : "NHWC")
             << " 输入期待尺寸:" << in_w_ << "x" << in_h_;
    QElapsedTimer tTotal; tTotal.start();

    cv::Mat input = preprocess(bgr);

    // HWC -> (N, C, H, W) 或 (N, H, W, C)
    std::vector<float> tensor_data;
    std::array<int64_t,4> input_shape{1, 3, in_h_, in_w_}; // 默认 NCHW

    if (layout_ == Layout::NCHW) {
        tensor_data.resize(3 * in_h_ * in_w_);
        std::vector<cv::Mat> chs(3);
        for (int c = 0; c < 3; ++c)
            chs[c] = cv::Mat(in_h_, in_w_, CV_32F, tensor_data.data() + c*in_h_*in_w_);
        cv::split(input, chs);
        input_shape = {1, 3, in_h_, in_w_};
    } else { // NHWC
        tensor_data.assign((float*)input.datastart, (float*)input.dataend);
        input_shape = {1, in_h_, in_w_, 3};
    }
    qDebug() << "[PlantDisease] 组装输入张量完成。shape = {"
             << input_shape[0] << "," << input_shape[1] << ","
             << input_shape[2] << "," << input_shape[3] << "}";

    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value in_tensor = Ort::Value::CreateTensor<float>(
        mem_info, tensor_data.data(), tensor_data.size(), input_shape.data(), 4
        );

    const char* in_names[]  = { input_name_.c_str() };
    const char* out_names[] = { output_name_.c_str() };

    QElapsedTimer tRun; tRun.start();
    auto outputs = session_.Run(Ort::RunOptions{nullptr}, in_names, &in_tensor, 1, out_names, 1);
    qDebug() << "[PlantDisease] ONNX Run 用时 (ms):" << tRun.elapsed();

    if (outputs.empty() || !outputs.front().IsTensor())
        throw std::runtime_error("Model did not return a tensor output");

    auto& out0 = outputs.front();
    float* out_data = out0.GetTensorMutableData<float>();
    size_t out_dim  = out0.GetTensorTypeAndShapeInfo().GetElementCount();
    if (out_dim == 0) throw std::runtime_error("Empty output tensor");

    qDebug() << "[PlantDisease] 输出元素数:" << static_cast<qulonglong>(out_dim);

    std::vector<float> logits(out_data, out_data + out_dim);

    // 调试输出：打印原始 logits 分布
    float logit_max = *std::max_element(logits.begin(), logits.end());
    float logit_min = *std::min_element(logits.begin(), logits.end());
    float logit_mean = std::accumulate(logits.begin(), logits.end(), 0.f) / logits.size();
    qDebug() << "[PlantDisease] 原始 logits: max=" << logit_max
             << " min=" << logit_min << " mean=" << logit_mean;

    // 检查 logits 是否异常（全部接近 0 或全部相等说明模型未正确加载）
    float logit_std = 0;
    for (float v : logits) logit_std += (v - logit_mean) * (v - logit_mean);
    logit_std = std::sqrt(logit_std / logits.size());
    qDebug() << "[PlantDisease] logits 标准差 =" << logit_std;

    if (logit_std < 0.01) {
        qWarning() << "[PlantDisease] 警告：logits 标准差过小，模型可能未正确加载权重或输出异常！";
    }

    std::vector<float> probs = cfg_.apply_softmax ? softmax(logits) : logits;

    // 调试输出：打印 softmax 后的概率分布
    float prob_max = *std::max_element(probs.begin(), probs.end());
    float prob_second = 0;
    std::vector<float> probs_sorted = probs;
    std::sort(probs_sorted.rbegin(), probs_sorted.rend());
    if (probs_sorted.size() > 1) prob_second = probs_sorted[1];
    qDebug() << "[PlantDisease] Softmax 后：max_prob=" << prob_max
             << " second_prob=" << prob_second
             << " 差距=" << (prob_max - prob_second);

    auto res = topK(probs, topk);
    qDebug() << "[PlantDisease] infer 完成，总耗时 (ms):" << tTotal.elapsed()
             << " 返回结果数:" << int(res.size());
    for (const auto& r : res) {
        qDebug() << "  ->" << QString::fromStdString(r.name) << "score=" << r.score;
    }
    return res;
}

// ---------- infer from file ----------
std::vector<PlantDiseaseResult> PlantDisease::inferFromFile(const QString& path, int topk) {
    qDebug() << "[PlantDisease] inferFromFile 路径:" << path << " topk=" << topk;
    cv::Mat img = imreadQString(path);
    if (img.empty())
        throw std::runtime_error(std::string("Cannot load image: ")
                                 + path.toUtf8().constData());
    return infer(img, topk);
}
