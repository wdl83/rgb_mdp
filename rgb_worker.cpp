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
const char *const TORCH_SPARK_THRESHOLD = "torch_spark_threshold";

const char *const ADDR = "addr";
const char *const COUNT = "count";
const char *const DEVICE = "device";
const char *const FCODE = "fcode";
const char *const SLAVE = "slave";
const char *const TIMEOUT_MS = "timeout_ms";
const char *const VALUE = "value";

constexpr auto FCODE_RD_HOLDING_REGISTERS = 3;
constexpr auto FCODE_WR_REGISTER = 6;
constexpr auto FCODE_WR_REGISTERS = 16;

constexpr uint8_t FLAG_UPDATED = 0x1;
constexpr uint8_t FLAG_REFRESH = 0x2;
constexpr uint8_t FX_NONE = 0 << 4;
constexpr uint8_t FX_STATIC = 1 << 4;
constexpr uint8_t FX_FIRE = 2 << 4;
constexpr uint8_t FX_TORCH = 3 << 4;

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
    return {id, "/dev/ttyUSB0", 128};
}

using milliseconds = std::chrono::milliseconds;

int count(const uint8_t &){return 1;}

template <typename T>
int count(const std::vector<T> &seq) {return seq.size();}

template <typename T>
void add(
    const DeviceID &deviceID,
    json &output,
    uint16_t addr,
    T value,
    milliseconds timeout = milliseconds{100})
{
    const auto num = count(value);

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, 1 == num ? FCODE_WR_REGISTER : FCODE_WR_REGISTERS},
            {ADDR, addr},
            {TIMEOUT_MS, timeout.count()},
            {COUNT, count(value)},
            {VALUE, value}
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

    add(deviceID, output, addr, value);
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

    std::vector<uint8_t> rgbSeq123(123, 0);
    std::vector<uint8_t> rgbSeq114(114, 0);

    std::generate(
        std::begin(rgbSeq123), std::end(rgbSeq123),
        [&rgb, n = 0]() mutable {return rgb[n++ % 3];});

    std::generate(
        std::begin(rgbSeq114), std::end(rgbSeq114),
        [&rgb, n = 0]() mutable {return rgb[n++ % 3];});

    add(deviceID, output, deviceID.calcADDR(5), rgbSeq123, milliseconds{500});
    add(deviceID, output, deviceID.calcADDR(128), rgbSeq123, milliseconds{500});
    add(deviceID, output, deviceID.calcADDR(251), rgbSeq114, milliseconds{500});
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

    add(deviceID, output, deviceID.calcADDR(3), seq);
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
    addFPS(deviceID, input, output);

    if("solid_rgb" == mode)
    {
        addSolidRGB(deviceID, input, output);
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_STATIC | FLAG_REFRESH | FLAG_UPDATED);
    }
    else if("fx_fire" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_FIRE | FLAG_REFRESH | FLAG_UPDATED);
    }
    else if("fx_torch" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_TORCH | FLAG_REFRESH | FLAG_UPDATED);

        add(
            deviceID,
            input, output,
            TORCH_SPARK_THRESHOLD,
            deviceID.calcADDR(509));
    }
    else if("off" == mode)
    {
        add(
            deviceID,
            output,
            deviceID.calcADDR(2),
            FX_NONE | FLAG_REFRESH | FLAG_UPDATED);
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

    for(char c; -1 != (c = ::getopt(argc, argv, "ha:"));)
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
                    //std::cout << input.dump(2) << std::endl;
                    output.push_back(parse(input));
                }

                //std::cout << output.dump(2) << std::endl;
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
