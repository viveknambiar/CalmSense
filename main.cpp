#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <smartspectra/smartspectra.h>
#include <smartspectra/smartspectra_config.h>
#include <smartspectra/messages/metrics.h>

using namespace presage::smartspectra;

// Pre-scan video frames to record exact timestamps (in microseconds) when hands lift to ear/head zones
std::vector<int64_t> detect_hand_to_ear_timestamps(const std::string& video_path) {
    std::vector<int64_t> trigger_times_us;
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) return trigger_times_us;

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0 || fps > 120) fps = 30.0;

    cv::Mat frame, gray, prev_gray, diff, thresh, ycrcb, skin_mask;
    int frame_idx = 0;
    int active_streak = 0;

    while (cap.read(frame)) {
        int64_t ts_us = static_cast<int64_t>((frame_idx / fps) * 1'000'000.0);
        frame_idx++;

        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(11, 11), 0);

        // Skin-tone mask in YCrCb space (detects hands even when held still over ears)
        cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
        cv::inRange(ycrcb, cv::Scalar(0, 133, 77), cv::Scalar(255, 173, 127), skin_mask);

        int w = frame.cols;
        int h = frame.rows;
        // Wide left & right ear/temple zones
        cv::Rect left_ear(static_cast<int>(w * 0.10), static_cast<int>(h * 0.10),
                          static_cast<int>(w * 0.28), static_cast<int>(h * 0.55));
        cv::Rect right_ear(static_cast<int>(w * 0.62), static_cast<int>(h * 0.10),
                           static_cast<int>(w * 0.28), static_cast<int>(h * 0.55));

        double left_skin = cv::countNonZero(skin_mask(left_ear)) / static_cast<double>(left_ear.area());
        double right_skin = cv::countNonZero(skin_mask(right_ear)) / static_cast<double>(right_ear.area());

        double motion_act = 0.0;
        if (!prev_gray.empty()) {
            cv::absdiff(prev_gray, gray, diff);
            cv::threshold(diff, thresh, 18, 255, cv::THRESH_BINARY);
            double l_mot = cv::countNonZero(thresh(left_ear)) / static_cast<double>(left_ear.area());
            double r_mot = cv::countNonZero(thresh(right_ear)) / static_cast<double>(right_ear.area());
            motion_act = std::max(l_mot, r_mot);
        }

        // Trigger if hands move in ear zones OR skin area occupies ear zones
        if (motion_act > 0.035 || left_skin > 0.12 || right_skin > 0.12) {
            active_streak++;
        } else {
            active_streak = std::max(0, active_streak - 1);
        }

        if (active_streak >= 3) {
            trigger_times_us.push_back(ts_us);
        }

        prev_gray = gray.clone();
    }

    std::cout << "[OpenCV Pre-Scan] Found " << trigger_times_us.size()
              << " hand-to-ear frames across " << frame_idx << " total frames." << std::endl;
    return trigger_times_us;
}

int main() {
	// https://physiology.presagetech.com/auth/login?source=registered
    const std::string api_key = "YOUR_PRESAGE_API_KEY";
    const std::string video_path = "test_video.mp4";
    const std::string log_path = "vitals_stream.jsonl";

	if (api_key.empty() || api_key == "YOUR_PRESAGE_API_KEY") {
            throw std::runtime_error(
                "Missing Presage API key! Replace 'YOUR_PRESAGE_API_KEY' in main.cpp "
            );
        }
    if (!std::filesystem::exists(video_path)) {
        std::cerr << "[Error] Video file not found: " << std::filesystem::absolute(video_path) << std::endl;
        return 1;
    }

    {
        std::ofstream clear_file(log_path, std::ios::trunc);
    }

    // Pre-scan video once for exact hand-to-ear timestamps
    const auto hand_ear_timestamps = detect_hand_to_ear_timestamps(video_path);

    const double base_pulse = 78.0;
    const double base_breath = 18.0;
    const double base_rmssd = 48.0;
    const double base_stress = 95.0;

    SmartSpectraConfig config;
    config.api_key = api_key;
    config.requested_metrics = SmartSpectraConfig::BreathingMetrics();
    config.AddMetrics(SmartSpectraConfig::CardioMetrics());
    config.AddMetrics(SmartSpectraConfig::FaceMetrics());

    SmartSpectra spectra(config);

    spectra.SetOnError([](const SmartSpectraError& error) {
        std::cerr << "[SDK Error] " << error.message << std::endl;
    });

    spectra.SetOnMetrics([&](const Metrics& metrics, int64_t ts_us) {
        double pulse = 0.0;
        double breathing = 0.0;
        double rmssd = 0.0;
        bool blink_detected = false;

        if (metrics.has_cardio()) {
            if (!metrics.cardio().pulse_rate().empty()) {
                pulse = metrics.cardio().pulse_rate().rbegin()->value();
            }
            if (!metrics.cardio().hrv().empty()) {
                rmssd = metrics.cardio().hrv().rbegin()->rmssd();
            }
        }

        if (metrics.has_breathing() && !metrics.breathing().rate().empty()) {
            breathing = metrics.breathing().rate().rbegin()->value();
        }

        if (metrics.has_face() && !metrics.face().blinking().empty()) {
            blink_detected = metrics.face().blinking().rbegin()->detected();
        }

        // Check if current video timestamp is within 1.5 seconds of any detected hand-to-ear frame
        bool hands_to_face = false;
        for (int64_t event_ts : hand_ear_timestamps) {
            if (std::abs(ts_us - event_ts) <= 1'500'000) {
                hands_to_face = true;
                break;
            }
        }

        if (pulse > 0 && breathing > 0) {
            if (rmssd <= 0.0) {
                rmssd = std::max(20.0, base_rmssd - (pulse - base_pulse) * 0.8);
            }
            double stress_index = std::max(50.0, base_stress + (pulse - base_pulse) * 2.5);
            double blink_rate = blink_detected ? 24.0 : 14.0;

            bool stage1 = (rmssd < base_rmssd * 0.85) || (stress_index > base_stress * 1.20) || blink_detected;
            bool stage2 = stage1 && (breathing > base_breath * 1.15);
            bool stage3 = ((pulse > base_pulse * 1.15) && (breathing > base_breath * 1.20)) || hands_to_face;

            int stage = stage3 ? 3 : (stage2 ? 2 : (stage1 ? 1 : 0));

            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            std::ofstream out(log_path, std::ios::app);
            if (out.is_open()) {
                out << "{"
                    << "\"timestamp\":" << now_ms << ","
                    << "\"pulse\":" << pulse << ","
                    << "\"breathing\":" << breathing << ","
                    << "\"rmssd\":" << rmssd << ","
                    << "\"stress_index\":" << stress_index << ","
                    << "\"blink_rate\":" << blink_rate << ","
                    << "\"hands_to_face\":" << (hands_to_face ? "true" : "false") << ","
                    << "\"stage\":" << stage
                    << "}\n";
                out.flush();
            }

            std::cout << "[Appended] Stage " << stage
                      << " | Pulse: " << pulse << " BPM"
                      << " | Breath: " << breathing << " RPM"
                      << " | HandsToFace: " << (hands_to_face ? "YES" : "NO")
                      << std::endl;
        }
    });

    while (true) {
        const auto source_err = spectra.UseFile(video_path).Build();
        if (!source_err.ok()) {
            std::cerr << "Video source error: " << source_err.message << std::endl;
            return 1;
        }

        std::cout << "Analyzing video and appending vitals to " << log_path << " ..." << std::endl;
        if (const auto start_err = spectra.Start(); !start_err.ok()) {
            std::cerr << "Start failed: " << start_err.message << std::endl;
            return 1;
        }

        spectra.WaitUntilComplete();
        spectra.Stop();

        std::cout << "Pass complete. Looping again in 2 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}
