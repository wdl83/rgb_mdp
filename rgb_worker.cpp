#include <iostream>
#include <chrono>

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

const char *const BRIGHTNESS = "brightness";
const char *const FPS = "fps";
const char *const ID = "id";
const char *const MODE = "mode";
const char *const PAYLOAD = "payload";
const char *const RGB = "RGB";
const char *const SERVICE = "service";

const char *const TORCH_ADJ_H = "torch_adj_h";
const char *const TORCH_ADJ_V = "torch_adj_v";
const char *const TORCH_COLOR_COEFF = "torch_color_coeff";
const char *const TORCH_PASSIVE_RETENTION = "torch_passive_retention";
const char *const TORCH_SPARK_RETENTION = "torch_spark_retention";
const char *const TORCH_SPARK_THRESHOLD = "torch_spark_threshold";
const char *const TORCH_SPARK_TRANSFER = "torch_spark_transfer";

const char *const NOISE_SPEED_STEP = "noise_speed_step";
const char *const NOISE_SCALE = "noise_scale";

const char *const PALETTE_ID = "palette_id";

const char *const ADDR = "addr";
const char *const COUNT = "count";
const char *const DEVICE = "device";
const char *const FCODE = "fcode";
const char *const SLAVE = "slave";
const char *const VALUE = "value";

constexpr auto FCODE_WR_BYTES = 66;

constexpr uint8_t FLAG_UPDATED = 0x1;
constexpr uint8_t FLAG_REFRESH = 0x2;
// strip_fx : 4 bits
constexpr uint8_t FX_NONE = 0 << 4;
constexpr uint8_t FX_STATIC = 1 << 4;
constexpr uint8_t FX_FIRE = 2 << 4;
constexpr uint8_t FX_TORCH = 3 << 4;
constexpr uint8_t FX_NOISE = 4 << 4;

struct DeviceID
{
    DeviceID(std::string name, std::string dev, uint8_t slave):
        id{std::move(name)},
        device{std::move(dev)},
        slaveID{slave}
    {}

    uint16_t calcADDR(uint16_t offset) const
    {
        return 0x1000 + offset;
    }

    std::string id;
    std::string device;
    uint8_t slaveID;
};

DeviceID toDeviceID(std::string id)
{
    /* TODO: provide json config file */
    if("A" == id) return {id, "/dev/ttyUSB0", 128};
    else if("B" == id) return {id, "/dev/ttyUSB0", 129};
    else if("C" == id) return {id, "/dev/ttyUSB0", 130};
    else if("D" == id) return {id, "/dev/ttyUSB2", 131};
    else if("E" == id) return {id, "/dev/ttyUSB1", 132};
    else if("F" == id) return {id, "/dev/ttyUSB1", 133};
    else if("G" == id) return {id, "/dev/ttyUSB0", 136};
    ENSURE("not supported" && false, RuntimeError);
    return {std::string{}, std::string{}, 0};
}

template <typename T>
std::vector<T> toVector(T value)
{
    return std::vector<T>{value};
}

template <typename T>
std::vector<T> toVector(std::vector<T> value)
{
    return value;
}

template <typename T>
void add(
    const DeviceID &deviceID,
    json &output,
    uint16_t addr,
    T value,
    const char *comment)
{
    auto asVector = toVector(value);

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, addr},
            {COUNT, int(asVector.size())},
            {VALUE, asVector},
            {"comment", comment}
        });
}

void add(
    const DeviceID &deviceID,
    const json &input, json &output,
    const char *tag,
    uint16_t addr)
{
    if(!input.count(tag)) return;

    ENSURE(input[tag].is_number(), RuntimeError);

    const auto value = input[tag].get<int>();

    ENSURE(inRange<uint8_t>(value), RuntimeError);

    add(deviceID, output, addr, value, tag);
}

void addSolidRGB(const DeviceID &deviceID, const json &input, json &output)
{
    ENSURE(input.count(RGB), RuntimeError);
    ENSURE(input[RGB].is_array(), RuntimeError);

    auto rgb = input[RGB].get<std::vector<int>>();

    ENSURE(3 == rgb.size(), RuntimeError);
    ENSURE(inRange<uint8_t>(rgb[0]), RuntimeError);
    ENSURE(inRange<uint8_t>(rgb[1]), RuntimeError);
    ENSURE(inRange<uint8_t>(rgb[2]), RuntimeError);

    /* RGB -> GRB */
    std::swap(rgb[0], rgb[1]);

    std::vector<uint8_t> rgbSeq249(249, 0);
    std::vector<uint8_t> rgbSeq111(111, 0);

    std::generate(
        std::begin(rgbSeq249), std::end(rgbSeq249),
        [&rgb, n = 0]() mutable {return rgb[n++ % 3];});

    std::generate(
        std::begin(rgbSeq111), std::end(rgbSeq111),
        [&rgb, n = 0]() mutable {return rgb[n++ % 3];});

    add(deviceID, output, deviceID.calcADDR(0 + 5), rgbSeq249, "RGB[0-248]");
    add(deviceID, output, deviceID.calcADDR(0 + 5 + 249), rgbSeq111, "RGB[249-359]");
}

void addFPS(const DeviceID &deviceID, const json &input, json &output)
{
    if(!input.count(FPS)) return;

    ENSURE(input[FPS].is_number(), RuntimeError);

    const auto fps = input[FPS].get<int>();

    ENSURE(3 < fps, RuntimeError);
    ENSURE(241 > fps, RuntimeError);

    const uint16_t tmrValue = 1000000 / (fps << 2);

    std::vector<uint8_t> seq{uint8_t(tmrValue & 0xFF), uint8_t(tmrValue >> 8)};

    add(deviceID, output, deviceID.calcADDR(3), seq, "TMR1A");
}

void addTorchColorCoeff(const DeviceID &deviceID, const json &input, json &output)
{
    ENSURE(input.count(TORCH_COLOR_COEFF), RuntimeError);
    ENSURE(input[TORCH_COLOR_COEFF].is_array(), RuntimeError);

    auto rgbCoeff = input[TORCH_COLOR_COEFF].get<std::vector<int>>();

    ENSURE(3 == rgbCoeff.size(), RuntimeError);
    ENSURE(inRange<uint8_t>(rgbCoeff[0]), RuntimeError);
    ENSURE(inRange<uint8_t>(rgbCoeff[1]), RuntimeError);
    ENSURE(inRange<uint8_t>(rgbCoeff[2]), RuntimeError);

    /* RGB -> GRB */
    std::swap(rgbCoeff[0], rgbCoeff[1]);

    add(deviceID, output, deviceID.calcADDR(516), rgbCoeff, "RGB coeff");
}

json parse(const json &input)
{
    ENSURE(input.is_object(), RuntimeError);
    ENSURE(input.count(ID), RuntimeError);
    ENSURE(input[ID].is_string(), RuntimeError);

    const auto id = input[ID].get<std::string>();
    const auto deviceID = toDeviceID(id);

    ENSURE(input.count(MODE), RuntimeError);
    ENSURE(input[MODE].is_string(), RuntimeError);

    const auto mode = input[MODE].get<std::string>();

    json output
    {
        {ID, deviceID.id},
        {SERVICE, "modbus_master_" + deviceID.device},
        {
            PAYLOAD, json::array()
        }
    };

    add(deviceID, input, output, BRIGHTNESS, deviceID.calcADDR(370));
    add(deviceID, input, output, PALETTE_ID, deviceID.calcADDR(377));
    addFPS(deviceID, input, output);

    if("solid_rgb" == mode)
    {
        addSolidRGB(deviceID, input, output);
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_STATIC | FLAG_REFRESH | FLAG_UPDATED,
            "flags");
    }
    else if("fx_fire" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_FIRE | FLAG_REFRESH | FLAG_UPDATED,
            "flags");
    }
    else if("fx_torch" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_TORCH | FLAG_REFRESH | FLAG_UPDATED,
            "flags");

        add(deviceID, input, output, TORCH_SPARK_THRESHOLD, deviceID.calcADDR(510));
        add(deviceID, input, output, TORCH_ADJ_H, deviceID.calcADDR(511));
        add(deviceID, input, output, TORCH_ADJ_V, deviceID.calcADDR(512));
        add(deviceID, input, output, TORCH_PASSIVE_RETENTION, deviceID.calcADDR(513));
        add(deviceID, input, output, TORCH_SPARK_TRANSFER, deviceID.calcADDR(514));
        add(deviceID, input, output, TORCH_SPARK_RETENTION, deviceID.calcADDR(515));
        addTorchColorCoeff(deviceID, input, output);
    }
    else if("fx_noise" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_NOISE | FLAG_REFRESH | FLAG_UPDATED,
            "flags");

        add(deviceID, input, output, NOISE_SPEED_STEP, deviceID.calcADDR(510));
        add(deviceID, input, output, NOISE_SCALE, deviceID.calcADDR(511));
    }
    else if("off" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_NONE | FLAG_REFRESH | FLAG_UPDATED,
            "flags");
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
        << std::endl;
}

int main(int argc, char *const argv[])
{
    std::string address;

    for(int c; -1 != (c = ::getopt(argc, argv, "ha:"));)
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
            case ':':
            case '?':
            default:
                help(argv[0], "geopt() failure");
                return EXIT_FAILURE;
                break;
        }
    }

    if(address.empty())
    {
        help(argv[0], "missing required arguments");
        return EXIT_FAILURE;
    }

    try
    {
        Worker{}.exec(
            address,
            "rgb",
            [](zmqpp::message message)
            {
                json output;

                for(auto i = 0u; i < message.parts(); ++i)
                {
                    auto input = json::parse(message.get<std::string>(i));

                    TRACE(TraceLevel::Info, "input ", input.dump());
                    output.push_back(parse(input));
                }

                TRACE(TraceLevel::Info, "output ", output.dump());
                return MDP::makeMessage(output.dump());
            });
    }
    catch(const std::exception &except)
    {
        std::cerr << "std exception " << except.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch(...)
    {
        std::cerr << "unsupported exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
