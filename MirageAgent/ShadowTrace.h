#pragma once

#include <Wbemidl.h>
#include <comdef.h>
#include <cctype>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

namespace ShadowTrace
{
	struct WmiFieldInfo
	{
		std::string className;
		std::string propertyName;
		bool unique = false;
	};

	struct WmiPropertySpec
	{
		const wchar_t* name;
		bool unique;
	};

	struct WmiClassSpec
	{
		const wchar_t* className;
		const WmiPropertySpec* props;
		size_t propCount;
	};

	static std::map<std::string, std::vector<WmiFieldInfo>> g_wmiFieldMap;
	static std::unordered_map<std::string, WmiFieldInfo> g_wmiUniqueNormalized;
	static std::unordered_map<std::string, std::string> g_wmiFakeCache;
	static std::unordered_map<std::string, std::string> g_wmiNonUniqueCache;
	static std::mutex g_wmiMutex;
	static std::mutex g_wmiRngMutex;
	static std::once_flag g_wmiInitOnce;
	static bool g_wmiInitOk = false;
	static std::atomic<uint64_t> g_wmiSeedCounter{ 0 };

	struct CpuProfile
	{
		const char* name;
		const char* manufacturer;
		const char* description;
	};

	static std::mt19937& WmiRng()
	{
		static std::mt19937 rng([]()
			{
				std::random_device rd;
				LARGE_INTEGER qpc = {};
				QueryPerformanceCounter(&qpc);
				const uint64_t tick = GetTickCount64();
				const uint64_t addr = reinterpret_cast<uint64_t>(&qpc);
				const uint64_t counter = g_wmiSeedCounter.fetch_add(1, std::memory_order_relaxed) + 1;
				std::seed_seq seq{
					static_cast<uint32_t>(rd()),
					static_cast<uint32_t>(rd()),
					static_cast<uint32_t>(qpc.LowPart),
					static_cast<uint32_t>(qpc.HighPart),
					static_cast<uint32_t>(tick),
					static_cast<uint32_t>(tick >> 32),
					static_cast<uint32_t>(GetCurrentProcessId()),
					static_cast<uint32_t>(GetCurrentThreadId()),
					static_cast<uint32_t>(addr),
					static_cast<uint32_t>(addr >> 32),
					static_cast<uint32_t>(counter),
					static_cast<uint32_t>(counter >> 32)
				};
				return std::mt19937(seq);
			}());
		return rng;
	}

	static size_t RandIndex(size_t maxExclusive)
	{
		if (maxExclusive == 0) return 0;
		std::lock_guard<std::mutex> lock(g_wmiRngMutex);
		std::uniform_int_distribution<size_t> dist(0, maxExclusive - 1);
		return dist(WmiRng());
	}

	static bool EqualsIC(const std::string& a, const std::string& b)
	{
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (to_lower_ascii(a[i]) != to_lower_ascii(b[i])) return false;
		}
		return true;
	}

	static std::string PickRandomValue(const std::vector<std::string>& list, const std::string& avoid = {})
	{
		if (list.empty()) return {};
		if (list.size() == 1) return list[0];
		for (size_t i = 0; i < 8; ++i)
		{
			const std::string& candidate = list[RandIndex(list.size())];
			if (!avoid.empty() && EqualsIC(candidate, avoid)) continue;
			return candidate;
		}
		return list[0];
	}

	static std::string PreserveTrailingSpaces(const std::string& orig, const std::string& repl)
	{
		size_t last = orig.find_last_not_of(' ');
		if (last == std::string::npos) return repl;
		size_t trimmedLen = last + 1;
		if (trimmedLen >= orig.size()) return repl;
		if (repl.size() >= trimmedLen)
		{
			std::string out = repl.substr(0, trimmedLen);
			out.append(orig.size() - trimmedLen, ' ');
			return out;
		}
		std::string out = repl;
		out.append(orig.size() - repl.size(), ' ');
		return out;
	}

	static std::string GenerateDigitsLike(const std::string& orig)
	{
		if (orig.empty()) return orig;
		std::string out = orig;
		for (size_t i = 0; i < out.size(); ++i)
		{
			char c = out[i];
			if (std::isdigit(static_cast<unsigned char>(c)))
			{
				out[i] = static_cast<char>('0' + (RandIndex(10)));
			}
		}
		return out;
	}

	static std::string GenerateLike(const std::string& orig, bool hexOnly)
	{
		std::string out = orig;
		for (size_t i = 0; i < out.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(out[i]);
			if (hexOnly && std::isxdigit(c))
			{
				const char* hex = std::islower(c) ? "0123456789abcdef" : "0123456789ABCDEF";
				out[i] = hex[RandIndex(16)];
			}
			else if (std::isdigit(c))
			{
				out[i] = static_cast<char>('0' + RandIndex(10));
			}
			else if (std::isalpha(c))
			{
				out[i] = static_cast<char>((std::isupper(c) ? 'A' : 'a') + RandIndex(26));
			}
		}
		return out;
	}

	static bool LooksLikeWmiDateTime(const std::string& s)
	{
		if (s.size() < 25) return false;
		for (size_t i = 0; i < 14; ++i)
			if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
		if (s[14] != '.') return false;
		for (size_t i = 15; i < 21; ++i)
			if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
		if (s[21] != '+' && s[21] != '-') return false;
		for (size_t i = 22; i < 25; ++i)
			if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
		return true;
	}

	static std::string GenerateWmiDateTimeLike(const std::string& orig)
	{
		int year = 2018 + static_cast<int>(RandIndex(8));
		int month = 1 + static_cast<int>(RandIndex(12));
		int day = 1 + static_cast<int>(RandIndex(28));
		int hour = static_cast<int>(RandIndex(24));
		int minute = static_cast<int>(RandIndex(60));
		int second = static_cast<int>(RandIndex(60));
		char buf[64] = {};
		const char sign = (orig.size() > 21 && (orig[21] == '-' || orig[21] == '+')) ? orig[21] : '+';
		int offset = 0;
		if (orig.size() >= 25)
		{
			for (size_t i = 22; i < 25; ++i)
			{
				char c = orig[i];
				if (c >= '0' && c <= '9') offset = offset * 10 + (c - '0');
			}
		}
		snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d.000000%c%03d",
			year, month, day, hour, minute, second, sign, offset);
		return std::string(buf);
	}

	static bool LooksLikeDateMDY(const std::string& s)
	{
		return s.size() == 10 &&
			std::isdigit(static_cast<unsigned char>(s[0])) &&
			std::isdigit(static_cast<unsigned char>(s[1])) &&
			s[2] == '/' &&
			std::isdigit(static_cast<unsigned char>(s[3])) &&
			std::isdigit(static_cast<unsigned char>(s[4])) &&
			s[5] == '/' &&
			std::isdigit(static_cast<unsigned char>(s[6])) &&
			std::isdigit(static_cast<unsigned char>(s[7])) &&
			std::isdigit(static_cast<unsigned char>(s[8])) &&
			std::isdigit(static_cast<unsigned char>(s[9]));
	}

	static std::string GenerateDateMDYLike()
	{
		int year = 2018 + static_cast<int>(RandIndex(8));
		int month = 1 + static_cast<int>(RandIndex(12));
		int day = 1 + static_cast<int>(RandIndex(28));
		char buf[16] = {};
		snprintf(buf, sizeof(buf), "%02d/%02d/%04d", month, day, year);
		return std::string(buf);
	}

	static std::string GenerateGuidLike()
	{
		const char* hex = "0123456789ABCDEF";
		char buf[40] = {};
		int pos = 0;
		int groups[] = { 8,4,4,4,12 };
		for (int g = 0; g < 5; ++g)
		{
			if (g > 0) buf[pos++] = '-';
			for (int i = 0; i < groups[g]; ++i)
			{
				buf[pos++] = hex[RandIndex(16)];
			}
		}
		return std::string(buf, static_cast<size_t>(pos));
	}

	static const std::vector<std::string> kMotherboardVendors =
	{
		"Hewlett-Packard Corporation",
		"ASUSTeK COMPUTER INC.",
		"AlienWare Computer Corp.",
		"Lenovo Plate INC",
		"Legion 5 Tek Corp",
		"MSI Tech Computer",
		"Arduino Technologies",
		"Gigabyte Technology Co., Ltd.",
		"Intel Corporation",
		"ASRock Incorporation",
		"Biostar Microtech International Corp.",
		"EVGA Corporation",
		"Foxconn Electronics Inc.",
		"Elitegroup Computer Systems Co., Ltd.",
		"Supermicro",
		"Acer Incorporated",
		"Dell Inc.",
		"Sony Corporation",
		"Toshiba Corporation",
		"Fujitsu Limited",
		"Samsung Electronics Co., Ltd.",
		"MSI Computer Corp.",
		"Micro-Star International Co., Ltd.",
		"Pegatron Corporation",
		"Quanta Computer Inc.",
		"Wistron Corporation",
		"Tyan Computer Corporation",
		"Colorful Technology and Development Co., Ltd.",
		"Sapphire Technology Limited",
		"Razer Inc.",
		"CLEVO Co.",
		"ASUS",
		"Gigabyte Technology",
		"Lenovo",
		"ASRock",
		"HP",
		"Dell",
		"Apple Inc.",
		"HuananZHI",
		"Jetway Information Co., Ltd.",
		"Shuttle Inc."
	};

	static const std::vector<std::string> kBiosVendors =
	{
		"American Megatrends International, LLC.",
		"APT Electronics International, LLC.",
		"IBM Technologies International, Ltd.",
		"Ozark Corp International, LLC.",
		"Arduino International, Ltd.",
		"Micro-Star International Co., Ltd.",
		"Phoenix Technologies Ltd.",
		"Insyde Corp.",
		"Dell Inc.",
		"Hewlett-Packard",
		"Lenovo",
		"Apple Inc.",
		"ASUSTeK COMPUTER INC.",
		"Acer Incorporated",
		"Toshiba Corporation"
	};

	static const std::vector<std::string> kSystemModels =
	{
		"ASUS TUF Dash F15 FX516PM_HN024T",
		"DELL G5 15 5590",
		"HP Pavilion 15",
		"Lenovo Legion 5",
		"MSI GF65 Thin 10UE",
		"Acer Nitro 5",
		"ASUS ROG Strix G15",
		"ThinkPad X1 Carbon",
		"Surface Laptop 4",
		"ROG Zephyrus G14",
		"Inspiron 15 5510",
		"Predator Helios 300",
		"TUF Gaming A15",
		"IdeaPad Gaming 3",
		"OMEN 16",
		"ASUS",
		"TOSHIBA",
		"DELL",
		"ACER",
		"MSI",
		"HP",
		"Legion 5",
		"ROG Strix G17",
		"Latitude 5520",
		"ThinkBook 15 G2",
		"VivoBook 15"
	};

	static const std::vector<std::string> kBoardProducts =
	{
		"FX516PM",
		"PRIME Z390-A",
		"TUF B550-PLUS",
		"ROG STRIX X570-E",
		"B450M DS3H",
		"H510M-A",
		"Z690 AORUS ELITE",
		"X570 AORUS ELITE",
		"B660M-A",
		"X470 GAMING PLUS",
		"Z590-A PRO",
		"B550M DS3H",
		"Z790 EDGE WIFI"
	};

	static const std::vector<std::string> kBoardVersions =
	{
		"1.0",
		"1.1",
		"1.2",
		"2.0",
		"2.1",
		"2.2",
		"3.0",
		"4.0",
		"Rev 1.0",
		"Rev 1.1",
		"Rev 1.2"
	};

	static const std::vector<std::string> kKeyboardNames =
	{
		"Standard PS/2 Keyboard",
		"HID Keyboard Device",
		"USB Keyboard Device",
		"Standard 101/102-Key or Microsoft Natural PS/2 Keyboard",
		"PC/AT Enhanced Keyboard (101/102-Key)",
		"Standard PS/2 Keyboard (106/109 Key)"
	};

	static const std::vector<std::string> kKeyboardLayouts =
	{
		"00000409",
		"00000419",
		"00000407",
		"0000040C",
		"00000410",
		"00000809"
	};

	static const std::vector<std::string> kSoundDevices =
	{
		"Realtek High Definition Audio",
		"NVIDIA High Definition Audio",
		"Intel Smart Sound Technology Audio",
		"NVIDIA Virtual Audio Device (Wave Extensible) (WDM)",
		"High Definition Audio Device",
		"Conexant SmartAudio HD",
		"AMD High Definition Audio Device",
		"USB Audio Device",
		"Realtek(R) Audio"
	};

	static const std::vector<std::string> kSoundVendors =
	{
		"Realtek",
		"NVIDIA",
		"Intel(R) Corporation",
		"Conexant",
		"Advanced Micro Devices, Inc.",
		"Microsoft"
	};

	static const std::vector<std::string> kGpuNames =
	{
		"NVIDIA GeForce RTX 3060 Laptop GPU",
		"NVIDIA GeForce RTX 2060 Super",
		"NVIDIA GeForce RTX 2070 Super",
		"NVIDIA GeForce RTX 2080 Super",
		"NVIDIA GeForce RTX 3070",
		"NVIDIA GeForce RTX 3080",
		"NVIDIA GeForce RTX 3090",
		"NVIDIA GeForce RTX 4060",
		"NVIDIA GeForce RTX 4070",
		"NVIDIA GeForce RTX 4080",
		"NVIDIA GeForce RTX 4090",
		"NVIDIA GeForce GTX 1080 Ti",
		"NVIDIA GeForce GTX 1070 Ti",
		"NVIDIA GeForce GTX 1070",
		"NVIDIA GeForce GTX 1060",
		"NVIDIA GeForce GTX 1050 Ti",
		"NVIDIA GeForce GTX 1050",
		"NVIDIA GeForce GTX 1660 Ti",
		"NVIDIA GeForce GTX 1650 Super",
		"NVIDIA GeForce GTX 1650",
		"Intel(R) Iris(R) Xe Graphics",
		"Intel(R) UHD Graphics 630",
		"Intel(R) UHD Graphics 770",
		"Intel(R) Arc A750",
		"Intel(R) Arc A770",
		"AMD Radeon RX 6600",
		"AMD Radeon RX 6700 XT",
		"AMD Radeon RX 6800 XT",
		"AMD Radeon RX 7600",
		"AMD Radeon RX 7900 XTX",
		"AMD Radeon RX 5700 XT",
		"AMD Radeon RX 5600 XT",
		"AMD Radeon Vega 8 Graphics"
	};

	static const std::vector<std::string> kVideoDescriptions =
	{
		"GeForce Graphics Card",
		"Dedicated Graphics Adapter",
		"NVIDIA Graphics Processor",
		"Radeon Graphics Adapter",
		"Integrated Graphics Processor",
		"NVIDIA GPU with 6GB VRAM",
		"GeForce RTX GPU",
		"Discrete Graphics Controller",
		"Integrated Graphics Adapter"
	};

	static const std::vector<std::string> kAdapterDacTypes =
	{
		"Integrated RAMDAC",
		"External RAMDAC",
		"Digital RAMDAC",
		"Analog RAMDAC"
	};

	static const std::vector<std::string> kAdapterCompatibility =
	{
		"NVIDIA",
		"Intel Corporation",
		"Advanced Micro Devices, Inc.",
		"ATI Technologies Inc.",
		"Microsoft",
		"PCI Express x16",
		"PCI Express x8",
		"PCI Express x4",
		"PCI Express x1"
	};

	static const std::vector<std::string> kDiskModels =
	{
		"SAMSUNG MZVLQ1T0HBLB-00B00",
		"SAMSUNG MZVLB512HBJQ-00000",
		"WDC WD10SPZX-21Z10T0",
		"WDC WD20SPZX-00UA7T0",
		"ST1000LM035-1RK172",
		"ST2000LM007-1R8174",
		"KINGSTON SNV2S1000G",
		"KINGSTON SA2000M81000G",
		"Micron 2400_MTFDKBA1T0QFM",
		"Crucial P3 Plus 1TB",
		"Seagate BarraCuda 1TB",
		"TOSHIBA MQ04ABF100"
	};

	static const std::vector<std::string> kMemoryVendors =
	{
		"Samsung",
		"Micron Technology",
		"SK Hynix",
		"Kingston",
		"Corsair",
		"Crucial",
		"ADATA",
		"Patriot",
		"TeamGroup",
		"PNY"
	};

	static const std::vector<std::string> kMemoryPartNumbers =
	{
		"M471A1G44AB0-CWE",
		"4ATF1G64HZ-3G2E2",
		"HMA81GS6DJR8N-XN",
		"CT8G4SFS824A",
		"KVR26S19S8/8",
		"HX432S20IB2/8",
		"F4-3200C22S-8GRS",
		"PVS48G320C6S",
		"TED48G3200C22-S01"
	};

	static const CpuProfile kCpuProfiles[] =
	{
		{ "Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz", "Intel", "Intel64 Family 6 Model 158 Stepping 12" },
		{ "AMD Ryzen 5 3600 6-Core Processor", "AuthenticAMD", "AMD64 Family 23 Model 113 Stepping 2" },
		{ "Intel(R) Core(TM) i9-9900K CPU @ 4.20GHz", "Intel", "Intel64 Family 6 Model 158 Stepping 13" },
		{ "AMD A8-7410 APU with AMD Radeon R5 Graphics", "AuthenticAMD", "AMD64 Family 21 Model 100 Stepping 1" },
		{ "Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz", "Intel", "Intel64 Family 6 Model 158 Stepping 14" },
		{ "AMD Ryzen 7 3700X 8-Core Processor", "AuthenticAMD", "AMD64 Family 23 Model 113 Stepping 0" },
		{ "Intel(R) Core(TM) i7-10700K CPU @ 4.60GHz", "Intel", "Intel64 Family 6 Model 165 Stepping 5" },
		{ "AMD Ryzen 5 2400G with Radeon RX Vega 11 Graphics", "AuthenticAMD", "AMD64 Family 21 Model 103 Stepping 0" },
		{ "Intel(R) Core(TM) i5-4670K CPU @ 3.40GHz", "Intel", "Intel64 Family 6 Model 60 Stepping 9" },
		{ "Intel(R) Core(TM) i7-12700K", "Intel", "Intel64 Family 6 Model 151 Stepping 5" },
		{ "Intel(R) Core(TM) i9-13900K", "Intel", "Intel64 Family 6 Model 183 Stepping 2" },
		{ "Intel(R) Core(TM) i5-12400F", "Intel", "Intel64 Family 6 Model 151 Stepping 2" },
		{ "AMD Ryzen 9 5900X 12-Core Processor", "AuthenticAMD", "AMD64 Family 25 Model 33 Stepping 2" },
		{ "AMD Ryzen 7 5800X 8-Core Processor", "AuthenticAMD", "AMD64 Family 25 Model 33 Stepping 0" },
		{ "AMD Ryzen 9 7950X 16-Core Processor", "AuthenticAMD", "AMD64 Family 25 Model 97 Stepping 2" },
		{ "AMD Ryzen 5 5600X 6-Core Processor", "AuthenticAMD", "AMD64 Family 25 Model 33 Stepping 2" },
		{ "Intel(R) Core(TM) i5-10400F CPU @ 2.90GHz", "Intel", "Intel64 Family 6 Model 165 Stepping 3" },
		{ "Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz", "Intel", "Intel64 Family 6 Model 158 Stepping 10" }
	};

	static std::string TrimWmiValue(const std::string& value)
	{
		size_t start = 0;
		size_t end = value.size();
		while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
		while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
		return value.substr(start, end - start);
	}

	static bool IsPlaceholderWmiValue(const std::string& value)
	{
		const std::string trimmed = TrimWmiValue(value);
		if (trimmed.empty()) return true;
		if (findStringIC(trimmed, xorstr_("To Be Filled By O.E.M."))) return true;
		if (findStringIC(trimmed, xorstr_("To be filled by O.E.M."))) return true;
		if (findStringIC(trimmed, xorstr_("No Asset Tag"))) return true;
		if (findStringIC(trimmed, xorstr_("Default string"))) return true;
		if (findStringIC(trimmed, xorstr_("Unknown"))) return true;
		if (findStringIC(trimmed, xorstr_("None"))) return true;
		if (findStringIC(trimmed, xorstr_("System Serial Number"))) return true;
		if (findStringIC(trimmed, xorstr_("System Product Name"))) return true;
		if (findStringIC(trimmed, xorstr_("System manufacturer"))) return true;
		if (findStringIC(trimmed, xorstr_("Not Specified"))) return true;
		if (findStringIC(trimmed, xorstr_("00000000-0000-0000-0000-000000000000"))) return true;
		return false;
	}

	static std::string NormalizeWmiValue(const std::string& value)
	{
		std::string out;
		out.reserve(value.size());
		for (unsigned char c : value)
		{
			if (std::isalnum(c)) out.push_back(static_cast<char>(std::toupper(c)));
		}
		return out;
	}

	static std::string WideToUtf8(const wchar_t* wide)
	{
		if (!wide) return {};
		return CvWideToAnsi(std::wstring(wide));
	}

	static bool VariantToString(const VARIANT& var, std::string& out)
	{
		switch (var.vt)
		{
		case VT_BSTR:
			if (var.bstrVal != nullptr)
			{
				out = WideToUtf8(var.bstrVal);
				return true;
			}
			return false;
		case VT_I1: out = std::to_string(var.cVal); return true;
		case VT_UI1: out = std::to_string(var.bVal); return true;
		case VT_I2: out = std::to_string(var.iVal); return true;
		case VT_UI2: out = std::to_string(var.uiVal); return true;
		case VT_I4: out = std::to_string(var.lVal); return true;
		case VT_UI4: out = std::to_string(var.ulVal); return true;
		case VT_I8: out = std::to_string(var.llVal); return true;
		case VT_UI8: out = std::to_string(var.ullVal); return true;
		case VT_BOOL: out = (var.boolVal == VARIANT_TRUE) ? "1" : "0"; return true;
		case VT_NULL:
		case VT_EMPTY:
			return false;
		default:
			return false;
		}
	}

	static const CpuProfile& GetCpuProfileFor(const std::string& orig)
	{
		static const CpuProfile* profile = nullptr;
		if (profile) return *profile;

		const bool wantAmd = findStringIC(orig, xorstr_("AMD"));
		const bool wantIntel = findStringIC(orig, xorstr_("Intel"));

		std::vector<size_t> candidates;
		for (size_t i = 0; i < _countof(kCpuProfiles); ++i)
		{
			const CpuProfile& p = kCpuProfiles[i];
			if (wantAmd && (findStringIC(p.manufacturer, "AMD") || findStringIC(p.name, "AMD")))
				candidates.push_back(i);
			else if (wantIntel && (findStringIC(p.manufacturer, "Intel") || findStringIC(p.name, "Intel")))
				candidates.push_back(i);
		}
		if (candidates.empty())
		{
			for (size_t i = 0; i < _countof(kCpuProfiles); ++i)
				candidates.push_back(i);
		}
		profile = &kCpuProfiles[candidates[RandIndex(candidates.size())]];
		return *profile;
	}

	static std::string PickGpuNameFor(const std::string& orig)
	{
		if (findStringIC(orig, xorstr_("Intel")))
		{
			static const std::vector<std::string> intel =
			{
				"Intel(R) Iris(R) Xe Graphics",
				"Intel(R) UHD Graphics 630",
				"Intel(R) UHD Graphics 770",
				"Intel(R) Arc A750",
				"Intel(R) Arc A770"
			};
			return PickRandomValue(intel, orig);
		}
		if (findStringIC(orig, xorstr_("AMD")) || findStringIC(orig, xorstr_("Radeon")))
		{
			static const std::vector<std::string> amd =
			{
				"AMD Radeon RX 6600",
				"AMD Radeon RX 6700 XT",
				"AMD Radeon RX 7600",
				"AMD Radeon RX 7900 XTX",
				"AMD Radeon RX 5700 XT",
				"AMD Radeon Vega 8 Graphics"
			};
			return PickRandomValue(amd, orig);
		}
		static const std::vector<std::string> nvidia =
		{
			"NVIDIA GeForce RTX 3060 Laptop GPU",
			"NVIDIA GeForce RTX 2060 Super",
			"NVIDIA GeForce RTX 2070 Super",
			"NVIDIA GeForce RTX 2080 Super",
			"NVIDIA GeForce RTX 3070",
			"NVIDIA GeForce RTX 3080",
			"NVIDIA GeForce RTX 4070",
			"NVIDIA GeForce RTX 4080",
			"NVIDIA GeForce GTX 1080 Ti",
			"NVIDIA GeForce GTX 1070",
			"NVIDIA GeForce GTX 1060",
			"NVIDIA GeForce GTX 1660 Ti",
			"NVIDIA GeForce GTX 1650"
		};
		return PickRandomValue(nvidia, orig);
	}

	static std::string PickGpuVendorFor(const std::string& orig)
	{
		if (findStringIC(orig, xorstr_("Intel"))) return "Intel Corporation";
		if (findStringIC(orig, xorstr_("AMD")) || findStringIC(orig, xorstr_("Radeon"))) return "Advanced Micro Devices, Inc.";
		if (findStringIC(orig, xorstr_("NVIDIA")) || findStringIC(orig, xorstr_("GeForce"))) return "NVIDIA";
		return PickRandomValue(kAdapterCompatibility, orig);
	}

	static std::string PickSoundDeviceFor(const std::string& orig)
	{
		if (findStringIC(orig, xorstr_("Realtek"))) return "Realtek High Definition Audio";
		if (findStringIC(orig, xorstr_("NVIDIA"))) return "NVIDIA High Definition Audio";
		if (findStringIC(orig, xorstr_("Intel"))) return "Intel Smart Sound Technology Audio";
		if (findStringIC(orig, xorstr_("AMD"))) return "AMD High Definition Audio Device";
		if (findStringIC(orig, xorstr_("Conexant"))) return "Conexant SmartAudio HD";
		return PickRandomValue(kSoundDevices, orig);
	}

	static std::string PickSoundVendorFor(const std::string& orig)
	{
		if (findStringIC(orig, xorstr_("Realtek"))) return "Realtek";
		if (findStringIC(orig, xorstr_("NVIDIA"))) return "NVIDIA";
		if (findStringIC(orig, xorstr_("Intel"))) return "Intel(R) Corporation";
		if (findStringIC(orig, xorstr_("AMD"))) return "Advanced Micro Devices, Inc.";
		if (findStringIC(orig, xorstr_("Conexant"))) return "Conexant";
		if (findStringIC(orig, xorstr_("Microsoft"))) return "Microsoft";
		return PickRandomValue(kSoundVendors, orig);
	}

	static std::string PickNumericList(const std::vector<uint64_t>& list, const std::string& orig)
	{
		if (list.empty()) return orig;
		uint64_t value = list[RandIndex(list.size())];
		std::string out = std::to_string(value);
		if (!orig.empty() && orig[0] == '-') out = "-" + out;
		return out;
	}

	static std::string PickNumericListInt(const std::vector<int>& list, const std::string& orig)
	{
		if (list.empty()) return orig;
		int value = list[RandIndex(list.size())];
		std::string out = std::to_string(value);
		if (!orig.empty() && orig[0] == '-') out = "-" + out;
		return out;
	}

	static bool GenerateNonUniqueFor(const WmiFieldInfo& info, const std::string& orig, std::string& out)
	{
		if (IsPlaceholderWmiValue(orig)) return false;

		const std::string& cls = info.className;
		const std::string& prop = info.propertyName;

		if (cls == "Win32_BIOS")
		{
			if (prop == "Manufacturer") { out = PickRandomValue(kBiosVendors, orig); return true; }
			if (prop == "SMBIOSBIOSVersion") { out = GenerateLike(orig, false); return true; }
			if (prop == "ReleaseDate")
			{
				if (LooksLikeWmiDateTime(orig)) out = GenerateWmiDateTimeLike(orig);
				else if (LooksLikeDateMDY(orig)) out = GenerateDateMDYLike();
				else out = GenerateLike(orig, false);
				return true;
			}
		}
		if (cls == "Win32_BaseBoard")
		{
			if (prop == "Manufacturer") { out = PickRandomValue(kMotherboardVendors, orig); return true; }
			if (prop == "Product") { out = PickRandomValue(kBoardProducts, orig); return true; }
			if (prop == "Version") { out = PickRandomValue(kBoardVersions, orig); return true; }
		}
		if (cls == "Win32_SystemEnclosure")
		{
			if (prop == "Manufacturer") { out = PickRandomValue(kMotherboardVendors, orig); return true; }
		}
		if (cls == "Win32_ComputerSystemProduct")
		{
			if (prop == "Vendor") { out = PickRandomValue(kMotherboardVendors, orig); return true; }
			if (prop == "Name") { out = PickRandomValue(kSystemModels, orig); return true; }
			if (prop == "Version") { out = PickRandomValue(kBoardVersions, orig); return true; }
		}
		if (cls == "Win32_Keyboard")
		{
			if (prop == "Name" || prop == "Description") { out = PickRandomValue(kKeyboardNames, orig); return true; }
			if (prop == "Layout") { out = PickRandomValue(kKeyboardLayouts, orig); return true; }
			if (prop == "DeviceID") { out = GenerateLike(orig, false); return true; }
			if (prop == "NumberOfFunctionKeys")
			{
				static const std::vector<int> keys = { 12, 15, 19, 24 };
				out = PickNumericListInt(keys, orig);
				return true;
			}
		}
		if (cls == "Win32_PhysicalMemory")
		{
			if (prop == "Manufacturer") { out = PickRandomValue(kMemoryVendors, orig); return true; }
			if (prop == "PartNumber") { out = PickRandomValue(kMemoryPartNumbers, orig); return true; }
			if (prop == "BankLabel") { out = GenerateLike(orig, false); return true; }
			if (prop == "DeviceLocator")
			{
				static const std::vector<std::string> locs =
				{
					"Controller0-ChannelA",
					"Controller1-ChannelA-DIMM0",
					"DIMM 0",
					"DIMM 1",
					"DIMM A1",
					"DIMM B1",
					"ChannelA-DIMM0",
					"ChannelB-DIMM0"
				};
				out = PickRandomValue(locs, orig);
				return true;
			}
			if (prop == "Capacity")
			{
				static const std::vector<uint64_t> caps =
				{
					4294967296ULL,
					8589934592ULL,
					17179869184ULL,
					34359738368ULL,
					68719476736ULL,
					137438953472ULL,
					274877906944ULL
				};
				out = PickNumericList(caps, orig);
				return true;
			}
			if (prop == "DataWidth")
			{
				static const std::vector<int> widths = { 64, 72, 128 };
				out = PickNumericListInt(widths, orig);
				return true;
			}
		}
		if (cls == "Win32_SoundDevice")
		{
			if (prop == "Name" || prop == "ProductName") { out = PickSoundDeviceFor(orig); return true; }
			if (prop == "Manufacturer") { out = PickSoundVendorFor(orig); return true; }
		}
		if (cls == "Win32_VideoController")
		{
			if (prop == "Name") { out = PickGpuNameFor(orig); return true; }
			if (prop == "VideoProcessor") { out = PickRandomValue(kVideoDescriptions, orig); return true; }
			if (prop == "AdapterCompatibility") { out = PickGpuVendorFor(orig); return true; }
			if (prop == "AdapterDACType") { out = PickRandomValue(kAdapterDacTypes, orig); return true; }
			if (prop == "AdapterRAM")
			{
				static const std::vector<uint64_t> vram =
				{
					1073741824ULL,
					2147483648ULL,
					4294967296ULL,
					6442450944ULL,
					8589934592ULL,
					12884901888ULL,
					17179869184ULL
				};
				out = PickNumericList(vram, orig);
				return true;
			}
			if (prop == "DriverVersion") { out = GenerateLike(orig, false); return true; }
			if (prop == "DriverDate")
			{
				if (LooksLikeWmiDateTime(orig)) out = GenerateWmiDateTimeLike(orig);
				else if (LooksLikeDateMDY(orig)) out = GenerateDateMDYLike();
				else out = GenerateLike(orig, false);
				return true;
			}
		}
		if (cls == "Win32_Processor")
		{
			const CpuProfile& cpu = GetCpuProfileFor(orig);
			if (prop == "Name") { out = cpu.name; return true; }
			if (prop == "Manufacturer") { out = cpu.manufacturer; return true; }
			if (prop == "Description") { out = cpu.description; return true; }
			if (prop == "MaxClockSpeed" || prop == "CurrentClockSpeed")
			{
				static const std::vector<int> clocks = { 1800, 2200, 2600, 3000, 3300, 3600, 4200, 4600, 5000 };
				out = PickNumericListInt(clocks, orig);
				return true;
			}
			if (prop == "NumberOfCores")
			{
				static const std::vector<int> cores = { 2, 4, 6, 8, 10, 12, 16 };
				out = PickNumericListInt(cores, orig);
				return true;
			}
			if (prop == "NumberOfLogicalProcessors" || prop == "ThreadCount")
			{
				static const std::vector<int> threads = { 4, 8, 12, 16, 20, 24, 32 };
				out = PickNumericListInt(threads, orig);
				return true;
			}
			if (prop == "L2CacheSize")
			{
				static const std::vector<int> l2 = { 256, 512, 1024, 2048, 4096 };
				out = PickNumericListInt(l2, orig);
				return true;
			}
			if (prop == "L3CacheSize")
			{
				static const std::vector<int> l3 = { 4096, 8192, 12288, 16384, 24576 };
				out = PickNumericListInt(l3, orig);
				return true;
			}
			if (prop == "Family")
			{
				static const std::vector<int> fam = { 6, 21, 23, 25 };
				out = PickNumericListInt(fam, orig);
				return true;
			}
			if (prop == "Stepping")
			{
				static const std::vector<int> step = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
				out = PickNumericListInt(step, orig);
				return true;
			}
			if (prop == "AddressWidth" || prop == "DataWidth")
			{
				static const std::vector<int> widths = { 64, 32 };
				out = PickNumericListInt(widths, orig);
				return true;
			}
		}
		if (cls == "Win32_CacheMemory")
		{
			static const std::vector<std::string> cacheNames = { "L1 Cache", "L2 Cache", "L3 Cache" };
			if (prop == "Name" || prop == "Description") { out = PickRandomValue(cacheNames, orig); return true; }
			if (prop == "InstalledSize" || prop == "MaxCacheSize")
			{
				static const std::vector<int> sizes = { 256, 512, 1024, 2048, 4096, 8192, 12288, 16384, 24576, 32768, 61440 };
				out = PickNumericListInt(sizes, orig);
				return true;
			}
			if (prop == "BlockSize")
			{
				static const std::vector<int> blocks = { 32, 64, 128 };
				out = PickNumericListInt(blocks, orig);
				return true;
			}
			if (prop == "CacheType")
			{
				static const std::vector<int> types = { 3, 4, 5 };
				out = PickNumericListInt(types, orig);
				return true;
			}
			if (prop == "Associativity")
			{
				static const std::vector<int> assoc = { 2, 4, 8, 16, 32, 64, 255 };
				out = PickNumericListInt(assoc, orig);
				return true;
			}
			if (prop == "Level")
			{
				static const std::vector<int> levels = { 1, 2, 3 };
				out = PickNumericListInt(levels, orig);
				return true;
			}
		}
		if (cls == "Win32_DiskDrive")
		{
			if (prop == "Model") { out = PickRandomValue(kDiskModels, orig); return true; }
			if (prop == "PNPDeviceID") { out = GenerateLike(orig, false); return true; }
			if (prop == "FirmwareRevision") { out = GenerateLike(orig, false); return true; }
			if (prop == "InterfaceType")
			{
				static const std::vector<std::string> ifaces = { "NVMe", "SCSI", "IDE", "USB", "SATA" };
				out = PickRandomValue(ifaces, orig);
				return true;
			}
		}
		if (cls == "Win32_NetworkAdapter")
		{
			if (prop == "Name")
			{
				static const std::vector<std::string> adapters =
				{
					"Intel(R) Wi-Fi 6 AX201 160MHz",
					"Realtek PCIe GbE Family Controller",
					"Qualcomm Atheros QCA9377 Wireless Network Adapter",
					"Broadcom 802.11ac Network Adapter",
					"Killer(R) Wi-Fi 6 AX1650i",
					"TP-Link Gigabit PCI Express Adapter",
					"Intel(R) Ethernet Connection (7) I219-V",
					"Marvell Yukon 88E8056 PCI-E Gigabit Ethernet Controller"
				};
				out = PickRandomValue(adapters, orig);
				return true;
			}
			if (prop == "Manufacturer")
			{
				static const std::vector<std::string> vendors =
				{
					"Intel Corporation",
					"Realtek",
					"Qualcomm Atheros",
					"Broadcom",
					"Killer Networking",
					"TP-Link",
					"Marvell"
				};
				out = PickRandomValue(vendors, orig);
				return true;
			}
			if (prop == "GUID") { out = GenerateGuidLike(); return true; }
		}
		if (cls == "Win32_NetworkAdapterConfiguration")
		{
			if (prop == "SettingID") { out = GenerateGuidLike(); return true; }
		}
		if (cls == "Win32_OperatingSystem")
		{
			if (prop == "Version")
			{
				static const std::vector<std::string> vers = { "10.0.19045", "10.0.22000", "10.0.22631" };
				out = PickRandomValue(vers, orig);
				return true;
			}
			if (prop == "BuildNumber")
			{
				static const std::vector<std::string> builds = { "19045", "22000", "22631" };
				out = PickRandomValue(builds, orig);
				return true;
			}
			if (prop == "OSArchitecture")
			{
				static const std::vector<std::string> arch = { "64-bit", "32-bit" };
				out = PickRandomValue(arch, orig);
				return true;
			}
			if (prop == "InstallDate")
			{
				if (LooksLikeWmiDateTime(orig)) out = GenerateWmiDateTimeLike(orig);
				else if (LooksLikeDateMDY(orig)) out = GenerateDateMDYLike();
				else out = GenerateLike(orig, false);
				return true;
			}
		}
		return false;
	}

	static bool TryGenerateNonUnique(const std::string& original, WmiFieldInfo& outInfo, std::string& outFake)
	{
		std::vector<WmiFieldInfo> infos;
		{
			std::lock_guard<std::mutex> lock(g_wmiMutex);
			auto it = g_wmiFieldMap.find(original);
			if (it == g_wmiFieldMap.end()) return false;
			infos = it->second;
		}

		for (const auto& info : infos)
		{
			const std::string cacheKey = info.className + "." + info.propertyName + "|" + original;
			{
				std::lock_guard<std::mutex> lock(g_wmiMutex);
				auto it = g_wmiNonUniqueCache.find(cacheKey);
				if (it != g_wmiNonUniqueCache.end())
				{
					outInfo = info;
					outFake = it->second;
					return true;
				}
			}

			std::string fake;
			if (GenerateNonUniqueFor(info, original, fake))
			{
				fake = PreserveTrailingSpaces(original, fake);
				{
					std::lock_guard<std::mutex> lock(g_wmiMutex);
					g_wmiNonUniqueCache.emplace(cacheKey, fake);
				}
				outInfo = info;
				outFake = fake;
				return true;
			}
		}
		return false;
	}

	static void AddWmiValue(const std::string& value, const std::string& className, const std::string& propertyName, bool unique)
	{
		if (value.empty()) return;
		WmiFieldInfo info{ className, propertyName, unique };
		std::lock_guard<std::mutex> lock(g_wmiMutex);
		g_wmiFieldMap[value].push_back(info);
		if (unique && !IsPlaceholderWmiValue(value))
		{
			std::string normalized = NormalizeWmiValue(value);
			if (!normalized.empty() && g_wmiUniqueNormalized.find(normalized) == g_wmiUniqueNormalized.end())
			{
				g_wmiUniqueNormalized.emplace(normalized, info);
			}
		}
	}

	static void QueryWmiClass(IWbemServices* svc, const WmiClassSpec& spec)
	{
		if (!svc || !spec.className || !spec.props || spec.propCount == 0) return;

		std::wstring query = L"SELECT ";
		for (size_t i = 0; i < spec.propCount; ++i)
		{
			if (i > 0) query += L", ";
			query += spec.props[i].name;
		}
		query += L" FROM ";
		query += spec.className;

		IEnumWbemClassObject* pEnumerator = nullptr;
		HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query.c_str()),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
		if (FAILED(hr) || !pEnumerator) return;

		const std::string className = WideToUtf8(spec.className);

		IWbemClassObject* pObj = nullptr;
		ULONG uReturn = 0;
		while (pEnumerator)
		{
			HRESULT hrNext = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
			if (FAILED(hrNext) || uReturn == 0) break;

			for (size_t i = 0; i < spec.propCount; ++i)
			{
				const std::string propName = WideToUtf8(spec.props[i].name);
				VARIANT vtProp;
				VariantInit(&vtProp);
				if (SUCCEEDED(pObj->Get(spec.props[i].name, 0, &vtProp, 0, 0)))
				{
					std::string value;
					if (VariantToString(vtProp, value))
					{
						AddWmiValue(value, className, propName, spec.props[i].unique);
					}
				}
				VariantClear(&vtProp);
			}
			pObj->Release();
		}
		pEnumerator->Release();
	}

	static bool InitWmiCache()
	{
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const bool comInit = (hr == S_OK || hr == S_FALSE);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] WMI: CoInitializeEx failed: 0x%08X\n"), hr);
			return false;
		}

		hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
			RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
			nullptr, EOAC_NONE, nullptr);
		if (FAILED(hr) && hr != RPC_E_TOO_LATE)
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] WMI: CoInitializeSecurity failed: 0x%08X\n"), hr);
			if (comInit) CoUninitialize();
			return false;
		}

		IWbemLocator* pLoc = nullptr;
		hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
		if (FAILED(hr) || !pLoc)
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] WMI: CoCreateInstance failed: 0x%08X\n"), hr);
			if (comInit) CoUninitialize();
			return false;
		}

		IWbemServices* pSvc = nullptr;
		hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &pSvc);
		if (FAILED(hr) || !pSvc)
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] WMI: ConnectServer failed: 0x%08X\n"), hr);
			pLoc->Release();
			if (comInit) CoUninitialize();
			return false;
		}

		hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
			RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
		if (FAILED(hr))
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] WMI: CoSetProxyBlanket failed: 0x%08X\n"), hr);
		}

		static const WmiPropertySpec kComputerSystemProductProps[] =
		{
			{ L"UUID", true },
			{ L"IdentifyingNumber", true },
			{ L"Vendor", false },
			{ L"Name", false },
			{ L"Version", false }
		};
		static const WmiPropertySpec kBiosProps[] =
		{
			{ L"Manufacturer", false },
			{ L"SMBIOSBIOSVersion", false },
			{ L"ReleaseDate", false },
			{ L"SerialNumber", true }
		};
		static const WmiPropertySpec kBaseBoardProps[] =
		{
			{ L"Manufacturer", false },
			{ L"Product", false },
			{ L"Version", false },
			{ L"SerialNumber", true },
			{ L"AssetTag", true }
		};
		static const WmiPropertySpec kSystemEnclosureProps[] =
		{
			{ L"Manufacturer", false },
			{ L"SerialNumber", true },
			{ L"SMBIOSAssetTag", true }
		};
		static const WmiPropertySpec kKeyboardProps[] =
		{
			{ L"Name", false },
			{ L"Description", false },
			{ L"Layout", false },
			{ L"DeviceID", false },
			{ L"NumberOfFunctionKeys", false }
		};
		static const WmiPropertySpec kPhysicalMemoryProps[] =
		{
			{ L"BankLabel", false },
			{ L"Capacity", false },
			{ L"DataWidth", false },
			{ L"DeviceLocator", false },
			{ L"Manufacturer", false },
			{ L"SerialNumber", true },
			{ L"PartNumber", false }
		};
		static const WmiPropertySpec kSoundDeviceProps[] =
		{
			{ L"Name", false },
			{ L"Manufacturer", false },
			{ L"ProductName", false }
		};
		static const WmiPropertySpec kVideoControllerProps[] =
		{
			{ L"Name", false },
			{ L"AdapterCompatibility", false },
			{ L"AdapterDACType", false },
			{ L"AdapterRAM", false },
			{ L"VideoProcessor", false },
			{ L"DriverVersion", false },
			{ L"DriverDate", false }
		};
		static const WmiPropertySpec kProcessorProps[] =
		{
			{ L"Name", false },
			{ L"Manufacturer", false },
			{ L"ProcessorId", true },
			{ L"Description", false },
			{ L"MaxClockSpeed", false },
			{ L"CurrentClockSpeed", false },
			{ L"NumberOfCores", false },
			{ L"NumberOfLogicalProcessors", false },
			{ L"ThreadCount", false },
			{ L"L2CacheSize", false },
			{ L"L3CacheSize", false },
			{ L"Family", false },
			{ L"Stepping", false },
			{ L"AddressWidth", false },
			{ L"DataWidth", false }
		};
		static const WmiPropertySpec kCacheMemoryProps[] =
		{
			{ L"Name", false },
			{ L"Description", false },
			{ L"InstalledSize", false },
			{ L"MaxCacheSize", false },
			{ L"BlockSize", false },
			{ L"CacheType", false },
			{ L"Associativity", false },
			{ L"Level", false }
		};
		static const WmiPropertySpec kDiskDriveProps[] =
		{
			{ L"Model", false },
			{ L"SerialNumber", true },
			{ L"PNPDeviceID", true },
			{ L"FirmwareRevision", false },
			{ L"InterfaceType", false }
		};
		static const WmiPropertySpec kNetworkAdapterProps[] =
		{
			{ L"Name", false },
			{ L"Manufacturer", false },
			{ L"MACAddress", true },
			{ L"PermanentAddress", true },
			{ L"GUID", true }
		};
		static const WmiPropertySpec kNetworkAdapterConfigProps[] =
		{
			{ L"MACAddress", true },
			{ L"SettingID", true }
		};
		static const WmiPropertySpec kOperatingSystemProps[] =
		{
			{ L"SerialNumber", true },
			{ L"Version", false },
			{ L"BuildNumber", false },
			{ L"OSArchitecture", false },
			{ L"InstallDate", false }
		};

		static const WmiClassSpec kWmiClasses[] =
		{
			{ L"Win32_ComputerSystemProduct", kComputerSystemProductProps, _countof(kComputerSystemProductProps) },
			{ L"Win32_BIOS", kBiosProps, _countof(kBiosProps) },
			{ L"Win32_BaseBoard", kBaseBoardProps, _countof(kBaseBoardProps) },
			{ L"Win32_SystemEnclosure", kSystemEnclosureProps, _countof(kSystemEnclosureProps) },
			{ L"Win32_Keyboard", kKeyboardProps, _countof(kKeyboardProps) },
			{ L"Win32_PhysicalMemory", kPhysicalMemoryProps, _countof(kPhysicalMemoryProps) },
			{ L"Win32_SoundDevice", kSoundDeviceProps, _countof(kSoundDeviceProps) },
			{ L"Win32_VideoController", kVideoControllerProps, _countof(kVideoControllerProps) },
			{ L"Win32_Processor", kProcessorProps, _countof(kProcessorProps) },
			{ L"Win32_CacheMemory", kCacheMemoryProps, _countof(kCacheMemoryProps) },
			{ L"Win32_DiskDrive", kDiskDriveProps, _countof(kDiskDriveProps) },
			{ L"Win32_NetworkAdapter", kNetworkAdapterProps, _countof(kNetworkAdapterProps) },
			{ L"Win32_NetworkAdapterConfiguration", kNetworkAdapterConfigProps, _countof(kNetworkAdapterConfigProps) },
			{ L"Win32_OperatingSystem", kOperatingSystemProps, _countof(kOperatingSystemProps) }
		};

		for (const auto& spec : kWmiClasses)
		{
			QueryWmiClass(pSvc, spec);
		}

		pSvc->Release();
		pLoc->Release();
		if (comInit) CoUninitialize();

		return true;
	}

	static void InitWmiCacheOnce()
	{
		std::call_once(g_wmiInitOnce, []()
			{
				g_wmiInitOk = InitWmiCache();
				if (g_wmiInitOk)
				{
					LogInFile(LOG_NAME, xorstr_("[LOG] WMI cache initialized. Fields: %llu\n"),
						static_cast<unsigned long long>(g_wmiFieldMap.size()));
				}
				else
				{
					LogInFile(LOG_NAME, xorstr_("[ERROR] WMI cache initialization failed.\n"));
				}
			});
	}

	static bool IsUniqueWmiValue(const std::string& value, WmiFieldInfo* outInfo)
	{
		if (IsPlaceholderWmiValue(value)) return false;
		const std::string normalized = NormalizeWmiValue(value);
		std::lock_guard<std::mutex> lock(g_wmiMutex);
		auto it = g_wmiFieldMap.find(value);
		if (it != g_wmiFieldMap.end())
		{
			for (const auto& info : it->second)
			{
				if (info.unique)
				{
					if (outInfo) *outInfo = info;
					return true;
				}
			}
		}
		auto itNorm = g_wmiUniqueNormalized.find(normalized);
		if (itNorm != g_wmiUniqueNormalized.end())
		{
			if (outInfo) *outInfo = itNorm->second;
			return true;
		}
		return false;
	}

	static std::string GenerateFake(const std::string& orig)
	{
		std::string res = orig;
		const char chars[] = "0123456789ABCDEF";

		for (size_t i = 0; i < res.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(res[i]);
			if (std::isxdigit(c)) res[i] = chars[RandIndex(16)];
			else if (std::isalpha(c)) res[i] = static_cast<char>('A' + RandIndex(26));
		}
		return res;
	}

	static std::string GetOrGenerateFake(const std::string& orig)
	{
		std::lock_guard<std::mutex> lock(g_wmiMutex);
		auto it = g_wmiFakeCache.find(orig);
		if (it != g_wmiFakeCache.end()) return it->second;
		std::string fake = GenerateFake(orig);
		g_wmiFakeCache.emplace(orig, fake);
		return fake;
	}

	static std::mutex g_macMutex;
	static std::unordered_map<std::string, std::string> g_macFakeCache;
	static std::unordered_set<std::string> g_macFakeUsed;

	static bool LooksLikeHexMac(const std::string& s, char* sepOut = nullptr, bool* upperOut = nullptr)
	{
		if (s.empty()) return false;
		char sep = 0;
		bool hasUpper = false;
		bool hasLower = false;
		size_t hexCount = 0;

		for (char c : s)
		{
			if (std::isxdigit(static_cast<unsigned char>(c)))
			{
				++hexCount;
				if (std::isalpha(static_cast<unsigned char>(c)))
				{
					if (std::isupper(static_cast<unsigned char>(c))) hasUpper = true;
					else hasLower = true;
				}
			}
			else if (c == ':' || c == '-')
			{
				if (sep == 0) sep = c;
				else if (sep != c) return false;
			}
			else
			{
				return false;
			}
		}

		if (hexCount != 12) return false;
		if (sep != 0 && s.size() != 17) return false;

		if (sepOut) *sepOut = sep;
		if (upperOut) *upperOut = (hasUpper && !hasLower);
		return true;
	}

	static std::string GenerateRandomMacLike(const std::string& orig)
	{
		char sep = 0;
		bool upper = false;
		if (!LooksLikeHexMac(orig, &sep, &upper)) return orig;

		unsigned char mac[6] = {};
		for (int i = 0; i < 6; ++i)
			mac[i] = static_cast<unsigned char>(ShadowTrace::RandIndex(256));

		mac[0] = static_cast<unsigned char>((mac[0] & 0xFE) | 0x02);

		static const char* hexLower = "0123456789abcdef";
		static const char* hexUpper = "0123456789ABCDEF";
		const char* hex = upper ? hexUpper : hexLower;

		std::string out;
		out.reserve(sep ? 17 : 12);
		for (int i = 0; i < 6; ++i)
		{
			if (i && sep) out.push_back(sep);
			out.push_back(hex[(mac[i] >> 4) & 0xF]);
			out.push_back(hex[mac[i] & 0xF]);
		}
		return out;
	}

	static std::string GetOrGenerateMacFake(const std::string& orig)
	{
		std::lock_guard<std::mutex> lock(g_macMutex);
		auto it = g_macFakeCache.find(orig);
		if (it != g_macFakeCache.end()) return it->second;
		std::string fake;
		for (int i = 0; i < 16; ++i)
		{
			fake = GenerateRandomMacLike(orig);
			if (fake.empty()) break;
			if (g_macFakeUsed.find(fake) == g_macFakeUsed.end()) break;
		}
		if (!fake.empty()) g_macFakeUsed.insert(fake);
		g_macFakeCache.emplace(orig, fake);
		return fake;
	}
}
