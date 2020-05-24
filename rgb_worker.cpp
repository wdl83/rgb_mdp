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

/* solid RGB format
 *
 * INPUT:
 * [
 *      {
 *          "id" : "A",
 *          "mode" : "solid_rgb",
 *          "RGB" : [255, 255, 255]
 *      },
 *      ...
 * ]
 *
 * number of elements in OUTPUT array will equal to number of elements in INPUT array
 *
 * OUTPUT:
 *  [
 *      {
 *         "id" : "A",
 *         "service" : "modbus_master_/dev/ttyUSB0",
 *          [
 *              {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 16,
 *                 "addr" : 4221,
 *                 "timeout_ms" : 500,
 *                 "count" : 123,
 *                 "value" : [ ... 123 RGB values ]
 *             },
 *             {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 16,
 *                 "addr" : 4344,
 *                 "timeout_ms" : 500,
 *                 "count" : 123,
 *                 "value" : [ ... 123 RGB values ]
 *             },
 *             {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 16,
 *                 "addr" : 4467,
 *                 "timeout_ms" : 500,
 *                 "count" : 114,
 *                 "value" : [ ... 114 RGB values ]
 *             },
 *             {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 6,
 *                 "addr" : 4098,
 *                 "timeout_ms" : 100,
 *                 "value" : 19
 *             }
 *          ]
 *      },
 *      ...
 *  ]
 *
 * INPUT:
 * [
 *      {
 *          "id" : "A",
 *          "mode" : "fx_fire"
 *      },
 *      ...
 * ]
 *
 * number of elements in OUTPUT array will equal to number of elements in INPUT array
 *
 * OUTPUT:
 *  [
 *      {
 *         "id" : "A",
 *         "service" : "modbus_master_/dev/ttyUSB0",
 *          [
 *              {

 * *             {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 6,
 *                 "addr" : 4098,
 *                 "timeout_ms" : 100,
 *                 "value" : 35
 *             }
 *          ]
 *      },
 *      ...
 * ]
 *
 * INPUT:
 * [
 *      {
 *          "id" : "A",
 *          "mode" : "off"
 *      },
 *      ...
 * ]
 *
 * number of elements in OUTPUT array will equal to number of elements in INPUT array
 *
 * OUTPUT:
 *  [
 *      {
 *         "id" : "A",
 *         "service" : "modbus_master_/dev/ttyUSB0",
 *          [
 *              {

 * *             {
 *                 "device" : "/dev/ttyUSB0",
 *                 "slave" : 128,
 *                 "fcode" : 6,
 *                 "addr" : 4098,
 *                 "timeout_ms" : 100,
 *                 "value" : 3
 *             }
 *          ]
 *      },
 *      ...
 * ]
 * */

const char *const ID = "id";
const char *const MODE = "mode";
const char *const RGB = "RGB";

const char *const DEVICE = "device";
const char *const SLAVE = "slave";
const char *const FCODE = "fcode";
const char *const COUNT = "count";
const char *const ADDR = "addr";
const char *const VALUE = "value";
const char *const TIMEOUT_MS = "timeout_ms";

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
    return {id, "/dev/ttyUSB0", 128};
}

json parseSolidRGB(const DeviceID &deviceID, const json &input)
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

    return json
    {
        {"id", deviceID.id},
        {"service", "modbus_master_" + deviceID.device},
        {
            "payload",
            {
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTERS},
                    {ADDR, 4221},
                    {COUNT, 123},
                    {TIMEOUT_MS, 500},
                    {VALUE, rgbSeq123}
                },
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTERS},
                    {ADDR, 4344},
                    {COUNT, 123},
                    {TIMEOUT_MS, 500},
                    {VALUE, rgbSeq123}
                },
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTERS},
                    {ADDR, 4467},
                    {COUNT, 114},
                    {TIMEOUT_MS, 500},
                    {VALUE, rgbSeq114}
                },
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTER},
                    {ADDR, 4098},
                    {TIMEOUT_MS, 100},
                    {VALUE, 0x13}
                },
            }
        }
    };
}

json parseFxFire(const DeviceID &deviceID, const json &input)
{
    (void)input;

    return json
    {
        {"id", deviceID.id},
        {"service", "modbus_master_" + deviceID.device},
        {
            "payload",
            {
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTER},
                    {ADDR, 4098},
                    {TIMEOUT_MS, 100},
                    {VALUE, 0x23}
                },
            }
        }
    };
}

json parseOFF(const DeviceID &deviceID, const json &input)
{
    (void)input;

    return json
    {
        {"id", deviceID.id},
        {"service", "modbus_master_" + deviceID.device},
        {
            "payload",
            {
                {
                    {DEVICE, deviceID.device},
                    {SLAVE, deviceID.slaveID},
                    {FCODE, FCODE_WR_REGISTER},
                    {ADDR, 4098},
                    {TIMEOUT_MS, 100},
                    {VALUE, 0x03}
                },
            }
        }
    };
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

    if("solid_rgb" == mode)
    {
        return parseSolidRGB(deviceID, input);
    }
    else if("fx_fire" == mode)
    {
        return parseFxFire(deviceID, input);
    }
    else if("off" == mode)
    {
        return parseOFF(deviceID, input);
    }
    else ENSURE(false, RuntimeError);

    return {};
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
