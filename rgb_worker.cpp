#include <chrono>
#include <fstream>
#include <iostream>

#include <unistd.h>

#include <nlohmann/json.hpp>

#include "mdp/Worker.h"
#include "Ensure.h"


template <typename T, typename V>
bool inRange(V value)
{
    return
        std::numeric_limits<T>::min() <= value
        && std::numeric_limits<T>::max() >= value;
}

using json = nlohmann::json;

const char *const DEVICE = "device";
const char *const MMAP = "mmap";

const char *const ID = "id";
const char *const LOCATION = "location";
const char *const MMAP_ID = "mmap_id";
const char *const SLAVE = "slave";
const char *const STRIP_SIZE = "strip_size";

const char *const FLAGS = "flags";
const char *const BRIGHTNESS = "brightness";
const char *const PALETTE_ID = "palette_id";
const char *const RGB = "rgb";

const char *const TORCH_ADJ_H = "torch_adj_h";
const char *const TORCH_ADJ_V = "torch_adj_v";
const char *const TORCH_COLOR_COEFF = "torch_color_coeff";
const char *const TORCH_PASSIVE_RETENTION = "torch_passive_retention";
const char *const TORCH_SPARK_RETENTION = "torch_spark_retention";
const char *const TORCH_SPARK_THRESHOLD = "torch_spark_threshold";
const char *const TORCH_SPARK_TRANSFER = "torch_spark_transfer";

const char *const NOISE_SPEED_STEP = "noise_speed_step";
const char *const NOISE_SCALE = "noise_scale";

const char *const MODE = "mode";
const char *const PAYLOAD = "payload";
const char *const SERVICE = "service";

const char *const ADDR = "addr";
const char *const COUNT = "count";
const char *const FCODE = "fcode";
const char *const VALUE = "value";

constexpr auto FCODE_WR_BYTES = 66;

constexpr uint8_t FLAG_UPDATED = 0x1;
// strip_fx : 4 bits
constexpr uint8_t FX_NONE = 0 << 4;
constexpr uint8_t FX_STATIC = 1 << 4;
constexpr uint8_t FX_FIRE = 2 << 4;
constexpr uint8_t FX_TORCH = 3 << 4;
constexpr uint8_t FX_NOISE = 4 << 4;

struct Device
{
    std::string id_;
    std::string location_;
    uint8_t slave_;
    std::string mmapId_;
    json mmap_;
    int stripSize_;

    Device(const json &device, const json &mmap)
    {
        vENSURE(device.count(ID), TagMissingError, device.dump());
        vENSURE(device[ID].is_string(), TagFormatError, device.dump());

        id_ = device[ID].get<std::string>();

        vENSURE(device.count(LOCATION), TagMissingError, device.dump());
        vENSURE(device[LOCATION].is_string(), TagFormatError, device.dump());

        location_ = device[LOCATION].get<std::string>();

        vENSURE(device.count(SLAVE), TagMissingError, device.dump());
        vENSURE(device[SLAVE].is_number(), TagFormatError, device.dump());

        slave_ = device[SLAVE].get<int>();

        vENSURE(device.count(MMAP_ID), TagMissingError, device.dump());
        vENSURE(device[MMAP_ID].is_string(), TagFormatError, device.dump());

        mmapId_ = device[MMAP_ID].get<std::string>();

        vENSURE(device.count(STRIP_SIZE), TagMissingError, device.dump());
        vENSURE(device[STRIP_SIZE].is_number(), TagFormatError, device.dump());

        stripSize_ = device[STRIP_SIZE].get<int>();

        vENSURE(mmap.count(mmapId_), TagMissingError, mmapId_, ' ', mmap.dump());
        vENSURE(mmap[mmapId_].is_object(), TagFormatError, mmap.dump());

        mmap_ = mmap[mmapId_];
    }

    const std::string &id() const {return id_;}
    const std::string &location() const {return location_;}
    uint8_t slave() const {return slave_;}

    uint16_t addr(const char *tag, int offset = 0) const
    {
        vENSURE(mmap_.count(tag), TagMissingError, '[', tag, "] ", mmap_.dump());
        vENSURE(mmap_[tag].is_number(), TagFormatError, mmap_.dump());

        const auto base = mmap_[tag].get<int>();

        vENSURE(inRange<uint16_t>(0x1000 + base + offset), TagValueRangeError, offset);
        return UINT16_C(0x1000) + uint16_t(base) + uint16_t(offset);
    }

    int stripSize() const {return stripSize_;}
};

using DeviceSeq = std::vector<Device>;

DeviceSeq parseDeviceSeq(const json &input)
{
    vENSURE(input.count(DEVICE), TagMissingError, input.dump());
    vENSURE(input[DEVICE].is_array(), TagFormatError, input.dump());

    vENSURE(input.count(MMAP), TagMissingError, input.dump());
    vENSURE(input[MMAP].is_object(), TagFormatError, input.dump());

    DeviceSeq seq;

    for(const auto &i : input[DEVICE].get<std::vector<json>>())
    {
        seq.emplace_back(i, input[MMAP]);
    }
    return seq;
}

using ByteSeq = std::vector<uint8_t>;

template <typename T>
ByteSeq toByteSeq(T value)
{
    vENSURE(inRange<uint8_t>(value), TagValueRangeError, value);
    return ByteSeq{uint8_t(value)};
}

template <typename T>
ByteSeq toByteSeq(std::vector<T> value)
{
    ByteSeq byteSeq;

    for(auto i : value)
    {
        vENSURE(inRange<uint8_t>(i), TagValueRangeError, i);
        byteSeq.push_back(i);
    }

    return byteSeq;
}

template <typename T>
void add(
    const Device &device,
    json &output,
    uint16_t addr,
    T value,
    const char *comment)
{
    auto byteSeq = toByteSeq(value);

    output[PAYLOAD].push_back(
        {
            {DEVICE, device.location()},
            {SLAVE, device.slave()},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, addr},
            {COUNT, int(byteSeq.size())},
            {VALUE, byteSeq},
            {"comment", comment}
        });
}

void addU8(
    const Device &device,
    const json &input, json &output,
    const char *tag)
{
    vENSURE(input.count(tag), TagMissingError, tag, ' ', input.dump());
    vENSURE(input[tag].is_number(), TagFormatError, input.dump());

    const auto value = input[tag].get<int>();

    vENSURE(inRange<uint8_t>(value), TagValueRangeError, value);

    add(device, output, device.addr(tag), value, tag);
}

void addRGB(const Device &device, const json &input, json &output)
{
    vENSURE(input.count(RGB), TagMissingError, input.dump());
    vENSURE(input[RGB].is_array(), TagFormatError, input.dump());

    auto rgb = input[RGB].get<std::vector<int>>();

    vENSURE(3 == rgb.size(), TagValueRangeError, rgb.size());
    vENSURE(inRange<uint8_t>(rgb[0]), TagValueRangeError, rgb[0]);
    vENSURE(inRange<uint8_t>(rgb[1]), TagValueRangeError, rgb[1]);
    vENSURE(inRange<uint8_t>(rgb[2]), TagValueRangeError, rgb[2]);

    /* RGB -> GRB (WS2812B native format) */
    std::swap(rgb[0], rgb[1]);

    std::vector<uint8_t> rgbSeq(device.stripSize() * 3, 0);

    std::generate(
        std::begin(rgbSeq), std::end(rgbSeq),
        [&rgb, n = 0]() mutable {return rgb[n++ % 3];});

    auto begin = std::begin(rgbSeq);
    const auto end = std::end(rgbSeq);

    while(begin != end)
    {
        auto next = std::next(begin, std::min(249, int(std::distance(begin, end))));
        const int offset = std::distance(std::begin(rgbSeq), begin);

        add(device, output, device.addr(RGB, offset), ByteSeq(begin, next), "rgb");
        begin = next;
    }
}

void addTorchColorCoeff(const Device &device, const json &input, json &output)
{
    vENSURE(input.count(TORCH_COLOR_COEFF), TagMissingError, input.dump());
    vENSURE(input[TORCH_COLOR_COEFF].is_array(), TagFormatError, input.dump());

    auto rgbCoeff = input[TORCH_COLOR_COEFF].get<std::vector<int>>();

    vENSURE(3 == rgbCoeff.size(), TagValueRangeError, rgbCoeff.size());
    vENSURE(inRange<uint8_t>(rgbCoeff[0]), TagValueRangeError, rgbCoeff[0]);
    vENSURE(inRange<uint8_t>(rgbCoeff[1]), TagValueRangeError, rgbCoeff[1]);
    vENSURE(inRange<uint8_t>(rgbCoeff[2]), TagValueRangeError, rgbCoeff[2]);

    /* RGB -> GRB */
    std::swap(rgbCoeff[0], rgbCoeff[1]);

    add(device, output, device.addr(TORCH_COLOR_COEFF), rgbCoeff, "RGB coeff");
}

json parse(const DeviceSeq &deviceSeq, const json &input)
{
    ENSURE(input.is_object(), RuntimeError);
    vENSURE(input.count(ID), TagMissingError, input.dump());
    vENSURE(input[ID].is_string(), TagMissingError, input.dump());

    const auto id = input[ID].get<std::string>();

    auto device =
        std::find_if(
            std::begin(deviceSeq), std::end(deviceSeq),
            [&](const Device &dev){return dev.id() == id;});

    ENSURE(device != std::end(deviceSeq), RuntimeError);

    vENSURE(input.count(MODE), TagMissingError, input.dump());
    vENSURE(input[MODE].is_string(), TagFormatError, input.dump());

    const auto mode = input[MODE].get<std::string>();

    json output
    {
        {ID, id},
        {SERVICE, "modbus_master_/" + device->location()},
        {
            PAYLOAD, json::array()
        }
    };

    addU8(*device, input, output, BRIGHTNESS);
    addU8(*device, input, output, PALETTE_ID);

    if("solid_rgb" == mode)
    {
        addRGB(*device, input, output);
        add(*device, output, device->addr(FLAGS), FX_STATIC | FLAG_UPDATED, FLAGS);
    }
    else if("fx_fire" == mode)
    {
        add(*device, output, device->addr(FLAGS), FX_FIRE | FLAG_UPDATED, FLAGS);
    }
    else if("fx_torch" == mode)
    {
        addU8(*device, input, output, TORCH_SPARK_THRESHOLD);
        addU8(*device, input, output, TORCH_ADJ_H);
        addU8(*device, input, output, TORCH_ADJ_V);
        addU8(*device, input, output, TORCH_PASSIVE_RETENTION);
        addU8(*device, input, output, TORCH_SPARK_TRANSFER);
        addU8(*device, input, output, TORCH_SPARK_RETENTION);
        addTorchColorCoeff(*device, input, output);
        add(*device, output, device->addr(FLAGS), FX_TORCH | FLAG_UPDATED, FLAGS);
    }
    else if("fx_noise" == mode)
    {
        addU8(*device, input, output, NOISE_SPEED_STEP);
        addU8(*device, input, output, NOISE_SCALE);
        add(*device, output, device->addr(FLAGS), FX_NOISE | FLAG_UPDATED, FLAGS);
    }
    else if("off" == mode)
    {
        add(*device, output, device->addr(FLAGS), FX_NONE | FLAG_UPDATED, FLAGS);
    }
    else ENSURE(false, RuntimeError);

    return output;
}

void help(const char *argv0, const char *message = nullptr)
{
    if(message) std::cout << "WARNING: " << message << '\n';

    std::cout
        << argv0
        << " -a broker_address"
        << " -c config"
        << std::endl;
}

int main(int argc, char *const argv[])
{
    std::string address;
    std::string config;

    for(int c; -1 != (c = ::getopt(argc, argv, "ha:c:"));)
    {
        switch(c)
        {
            case 'h':
                help(argv[0]);
                return EXIT_SUCCESS;
                break;
            case 'a':
                address = optarg ? optarg : "";
                break;
            case 'c':
                config = optarg ? optarg : "";
                break;
            case ':':
            case '?':
            default:
                help(argv[0], "geopt() failure");
                return EXIT_FAILURE;
                break;
        }
    }

    if(address.empty() || config.empty())
    {
        help(argv[0], "missing required arguments");
        return EXIT_FAILURE;
    }

    try
    {
        DeviceSeq deviceSeq;

        {
            json input;
            std::ifstream{config} >> input;
            deviceSeq = parseDeviceSeq(input);
        }

        Worker{}.exec(
            address,
            "rgb",
            [&deviceSeq](zmqpp::message message)
            {
                json output;

                for(auto i = 0u; i < message.parts(); ++i)
                {
                    auto input = json::parse(message.get<std::string>(i));

                    TRACE(TraceLevel::Info, "input ", input.dump());
                    output.push_back(parse(deviceSeq, input));
                }

                TRACE(TraceLevel::Info, "output ", output.dump());
                return MDP::makeMessage(output.dump());
            });
    }
    catch(const std::exception &except)
    {
        TRACE(TraceLevel::Error, except.what());
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TRACE(TraceLevel::Error, "unsupported exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
