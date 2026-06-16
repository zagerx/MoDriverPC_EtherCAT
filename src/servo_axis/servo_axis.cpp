#include "servo_axis/servo_axis.h"

namespace mo_ecat {

AxisConfig MakeDefaultAxisConfig(int slave_id)
{
    AxisConfig cfg;
    cfg.name = "axis_" + std::to_string(slave_id);
    cfg.slave_id = slave_id;

    // 默认输出 PDO 映射（与架构文档示例一致）
    cfg.output_entries = {
        {"control_word", 0, 2},
        {"mode_of_operation", 2, 1},
        {"target_position", 4, 4},
        {"target_velocity", 8, 4},
        {"target_torque", 12, 2},
    };

    // 默认输入 PDO 映射
    cfg.input_entries = {
        {"status_word", 0, 2},
        {"mode_of_operation_display", 2, 1},
        {"position_actual_value", 4, 4},
        {"velocity_actual_value", 8, 4},
        {"torque_actual_value", 12, 2},
    };

    return cfg;
}

ServoAxis::ServoAxis(EcMaster &master, const AxisConfig &config)
    : master_(master), config_(config)
{
    output_buffer_.resize(ComputeBufferSize(config_.output_entries), 0);
    input_buffer_.resize(ComputeBufferSize(config_.input_entries), 0);
}

const AxisConfig &ServoAxis::GetConfig() const
{
    return config_;
}

const PdoEntry *ServoAxis::FindEntry(const std::vector<PdoEntry> &entries, const std::string &name)
{
    for (const auto &entry : entries) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

size_t ServoAxis::ComputeBufferSize(const std::vector<PdoEntry> &entries)
{
    size_t size = 0;
    for (const auto &entry : entries) {
        size = std::max(size, static_cast<size_t>(entry.offset + entry.size));
    }
    return size;
}

void ServoAxis::UpdatePdoOutput()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!output_buffer_.empty()) {
        master_.WriteOutput(config_.slave_id, 0, output_buffer_.data(),
                            static_cast<int>(output_buffer_.size()));
    }
}

void ServoAxis::UpdatePdoInput()
{
    std::vector<uint8_t> buffer = input_buffer_; // 先读到临时缓冲，减少加锁时间
    if (!buffer.empty()) {
        master_.ReadInput(config_.slave_id, 0, buffer.data(), static_cast<int>(buffer.size()));
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        input_buffer_.swap(buffer);
    }
}

// 显式实例化常用类型，避免模板只在头文件中定义导致的链接问题
// （实际模板函数已内联在头文件中，这里主要是为了文档清晰）

template bool ServoAxis::WriteOutput<int8_t>(const std::string &, const int8_t &);
template bool ServoAxis::WriteOutput<int16_t>(const std::string &, const int16_t &);
template bool ServoAxis::WriteOutput<int32_t>(const std::string &, const int32_t &);
template bool ServoAxis::WriteOutput<uint8_t>(const std::string &, const uint8_t &);
template bool ServoAxis::WriteOutput<uint16_t>(const std::string &, const uint16_t &);
template bool ServoAxis::WriteOutput<uint32_t>(const std::string &, const uint32_t &);

template bool ServoAxis::ReadInput<int8_t>(const std::string &, int8_t &) const;
template bool ServoAxis::ReadInput<int16_t>(const std::string &, int16_t &) const;
template bool ServoAxis::ReadInput<int32_t>(const std::string &, int32_t &) const;
template bool ServoAxis::ReadInput<uint8_t>(const std::string &, uint8_t &) const;
template bool ServoAxis::ReadInput<uint16_t>(const std::string &, uint16_t &) const;
template bool ServoAxis::ReadInput<uint32_t>(const std::string &, uint32_t &) const;

} // namespace mo_ecat
