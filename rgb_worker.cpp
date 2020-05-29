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
/* number of elements in OUTPUT array will equal to
 * number of elements in INPUT array
 *
 * INPUT:
 * [{
 *    "id" : "A",
 *    "mode" : "solid_rgb",
 *    "brightness" : brightness,
 *    "fps": fps,
 *    "RGB" : [255, 255, 255]
 *  }, ...]
 * OUTPUT:
 *  [{
 *     "id" : "A",
 *     "service" : "modbus_master_/dev/ttyUSB0",
 *    "payload":
 *     [
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 16,
 *         "addr" : 4101,
 *         "timeout_ms" : 500,
 *         "count" : 123,
 *         "value" : [ ... 123 RGB values ]
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 16,
 *         "addr" : 4224,
 *         "timeout_ms" : 500,
 *         "count" : 123,
 *         "value" : [ ... 123 RGB values ]
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 16,
 *         "addr" : 4347,
 *         "timeout_ms" : 500,
 *         "count" : 114,
 *         "value" : [ ... 114 RGB values ]
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 6,
 *         "addr" : 4466
 *         "timeout_ms" : 100,
 *         "value" : brightness
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 6,
 *         "addr" : 4099
 *         "timeout_ms" : 100,
 *         "value" : fps_low_byte
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 6,
 *         "addr" : 4100
 *         "timeout_ms" : 100,
 *         "value" : tmr1_A_high_byte
 *       },
 *       {
 *         "device" : "/dev/ttyUSB0",
 *         "slave" : 128,
 *         "fcode" : 6,
 *         "addr" : 4098,
 *         "timeout_ms" : 100,
 *         "value" : 19
 *       }
 *     ]
 *  }, ...]
 * INPUT:
 * [{
 *    "id" : "A",
 *    "mode" : "fx_fire | fx_torch | off"
 *  }, ...]
 * OUTPUT:
 * [{
 *    "id" : "A",
 *    "service" : "modbus_master_/dev/ttyUSB0",
 *    "payload":
 *    [
 *      {
 *        "device" : "/dev/ttyUSB0",
 *        "slave" : 128,
 *        "fcode" : 6,
 *        "addr" : 4098,
 *        "timeout_ms" : 100,
 *        "value" : 35 (fx_fire) | 51 (fx_torch) | 3 (off)
 *      }
 *    ]
 * },...]
 */

const char *const BRIGHTNESS = "brightness";
const char *const FPS = "fps";
const char *const ID = "id";
const char *const MODE = "mode";
const char *const PAYLOAD = "payload";
const char *const RGB = "RGB";
const char *const SERVICE = "service";

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

struct DeviceID
{
    DeviceID(std::string name, std::string dev, uint8_t slave):
        id{std::move(name)},
        device{std::move(dev)},
        slaveID{slave}
    {}

    std::string id;
    std::string device;
    uint8_t slaveID;
};

DeviceID toDeviceID(std::string id)
{
    /* TODO: provide json config file */
    return {id, "/dev/ttyUSB0", 128};
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

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTERS},
            {ADDR, 4101},
            {COUNT, 123},
            {TIMEOUT_MS, 500},
            {VALUE, rgbSeq123}
        });
    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTERS},
            {ADDR, 4224},
            {COUNT, 123},
            {TIMEOUT_MS, 500},
            {VALUE, rgbSeq123}
        });
    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTERS},
            {ADDR, 4347},
            {COUNT, 114},
            {TIMEOUT_MS, 500},
            {VALUE, rgbSeq114}
        });
}

void addMode(const DeviceID &deviceID, const json &input, json &output, uint8_t value)
{
    (void)input;

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTER},
            {ADDR, 4098},
            {TIMEOUT_MS, 100},
            {VALUE, value}
        });
}

void addBrightness(const DeviceID &deviceID, const json &input, json &output)
{
    if(!input.count(BRIGHTNESS)) return;

    ENSURE(input[BRIGHTNESS].is_number(), RuntimeError);

    const auto brightness = input[BRIGHTNESS].get<int>();

    ENSURE(inRange<uint8_t>(brightness), RuntimeError);

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTER},
            {ADDR, 4466},
            {TIMEOUT_MS, 100},
            {VALUE, brightness}
        });
}

void addFPS(const DeviceID &deviceID, const json &input, json &output)
{
    if(!input.count(FPS)) return;

    ENSURE(input[FPS].is_number(), RuntimeError);

    const auto fps = input[FPS].get<int>();

    ENSURE(3 < fps, RuntimeError);
    ENSURE(241 > fps, RuntimeError);

    const uint16_t tmrValue = 1000000 / (fps << 2);

    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTER},
            {ADDR, 4099},
            {TIMEOUT_MS, 100},
            {VALUE, uint8_t(tmrValue & 0xFF)}
        });
    output[PAYLOAD].push_back(
        {
            {DEVICE, deviceID.device},
            {SLAVE, deviceID.slaveID},
            {FCODE, FCODE_WR_REGISTER},
            {ADDR, 4100},
            {TIMEOUT_MS, 100},
            {VALUE, uint8_t(tmrValue >> 8)}
        });
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

    addBrightness(deviceID, input, output);
    addFPS(deviceID, input, output);

    if("solid_rgb" == mode)
    {
        addSolidRGB(deviceID, input, output);
        addMode(deviceID, input, output, 0x13);
    }
    else if("fx_fire" == mode)
    {
        addMode(deviceID, input, output, 0x23);
    }
    else if("fx_torch" == mode)
    {
        addMode(deviceID, input, output, 0x33);
    }
    else if("off" == mode)
    {
        addMode(deviceID, input, output, 0x03);
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
