#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <filesystem>

constexpr auto plugin_version = "1.0";

enum class ProductQuality : uint8_t
{
	Common = 0,
	Uncommon = 1,
	Rare = 2,
	VeryRare = 3,
	Import = 4,
	Exotic = 5,
	BlackMarket = 6,
	Premium = 7,
	Limited = 8,
	Legacy = 9,
	MAX = 10
};

struct ProductQualityInfo
{
	std::string name;
};

struct ProductStruct
{
	int product_id = 0;
	std::string name = "";
	std::string slot = "";
	std::string paint = "none";
	std::string certification = "none";
	int certification_value = 0;
	std::string rank_label = "none";
	std::string special_edition = "none";
	std::string quality = "unknown";
	std::string crate = "";
	unsigned long long instance_id = 0;
	bool tradeable = 0;
	
	int blueprint_item_id = 0;
	std::string blueprint_item = "";
	int blueprint_cost = 0;

	int amount = 1;
};

class BetterInventoryExport: public BakkesMod::Plugin::BakkesModPlugin/*, public BakkesMod::Plugin::PluginWindow*/
{
	using HandleAttribute = std::function<void(ProductAttributeWrapper paw, ProductStruct& productData)>;
private:
	std::map<std::string, HandleAttribute> attributeHandler;
	const std::map<uint8_t, ProductQualityInfo> productQualities = {
		{(uint8_t)ProductQuality::Common, {"Common"}},
		{(uint8_t)ProductQuality::Uncommon, {"Uncommon"}},
		{(uint8_t)ProductQuality::Rare, {"Rare"}},
		{(uint8_t)ProductQuality::VeryRare, {"Very rare"}},
		{(uint8_t)ProductQuality::Import, {"Import"}},
		{(uint8_t)ProductQuality::Exotic, {"Exotic"}},
		{(uint8_t)ProductQuality::BlackMarket, {"Black market"}},
		{(uint8_t)ProductQuality::Premium, {"Premium"}},
		{(uint8_t)ProductQuality::Limited, {"Limited"}},
		{(uint8_t)ProductQuality::Legacy, {"Legacy"}},
	};
public:
	virtual void onLoad();
	virtual void onUnload();

	void OnInventDump(std::vector<std::string> params);

	ProductStruct GetProductInfo(ProductWrapper& pw);

	ProductStruct GetOnlineProductInfo(OnlineProductWrapper& pw);


	void export_csv(std::filesystem::path filename, std::vector<ProductStruct>& psv);
	void export_json(std::filesystem::path filename, std::vector<ProductStruct>& psv);
};

