#include "pch.h"
#include "BetterInventoryExport.h"
#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/Engine/UnrealStringWrapper.h"
#include "bakkesmod/wrappers/items/dbs/PaintDatabaseWrapper.h"
#include <fstream>

BAKKESMOD_PLUGIN(BetterInventoryExport, "Better inventory export", plugin_version, PLUGINTYPE_FREEPLAY)


void BetterInventoryExport::onLoad()
{
	using namespace std::placeholders;
	cvarManager->registerNotifier("invent_dump_better", std::bind(&BetterInventoryExport::OnInventDump, this, _1), "Invent dump v2", PERMISSION_ALL);
	
	attributeHandler["ProductAttribute_Painted_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData) 
	{
		ProductAttribute_PaintedWrapper papw(paw.memory_address);
		const int paintID = papw.GetPaintID();
		if (paintID >= 0 && paintID < paintNames.size())
		{
			productData.paint = paintNames.at(paintID);
		}
		else
		{
			productData.paint = std::to_string(papw.GetPaintID());
		}
	};
	
	attributeHandler["ProductAttribute_Certified_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData)
	{
		auto statDB = gameWrapper->GetItemsWrapper().GetCertifiedStatDB();
		
		ProductAttribute_CertifiedWrapper pacw(paw.memory_address);
		if (statDB.memory_address != NULL)
		{
			productData.certification = statDB.GetStatName(pacw.GetStatId());
		} 
		else
		{
			productData.certification = pacw.GetValueKeyName(); //Check if this is correct, please let it be so
		}
		productData.certification_value = pacw.GetStatValue();
		if (auto rankLabel = pacw.GetRankLabel(); !rankLabel.IsNull())
		{
			productData.rank_label = rankLabel.ToString();
		}
	};

	attributeHandler["ProductAttribute_SpecialEdition_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData)
	{
		auto specialEditionDB = gameWrapper->GetItemsWrapper().GetSpecialEditionDB();
		ProductAttribute_SpecialEditionWrapper pase(paw.memory_address);
		productData.product_id = pase.GetOverrideProductID(productData.product_id);
		if (specialEditionDB.memory_address != NULL)
		{
			productData.special_edition = specialEditionDB.GetSpecialEditionName(pase.GetEditionID());
		}
	};

	attributeHandler["ProductAttribute_Blueprint_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData)
	{
		auto itemWrapper = gameWrapper->GetItemsWrapper();
		ProductAttribute_BlueprintWrapper pabw(paw.memory_address);
	
		int productToUnlock = pabw.GetProductID();
		if (auto product = itemWrapper.GetProduct(productToUnlock); product.memory_address != NULL)
		{
			productData.blueprint_item_id = productToUnlock;
			if (auto productName = product.GetLongLabel(); !productName.IsNull())
			{
				productData.blueprint_item = productName.ToString();
			}
			if (auto displaySlot = product.GetDisplayLabelSlot(); !displaySlot.IsNull())
			{
				productData.slot = displaySlot.ToString();
			}
		}
	};

	attributeHandler["ProductAttribute_BlueprintCost_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData)
	{
		ProductAttribute_BlueprintCostWrapper pabcw(paw.memory_address);
		productData.blueprint_cost = pabcw.GetCost();
	};

	attributeHandler["ProductAttribute_Quality_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData)
	{
		ProductAttribute_QualityWrapper paqw(paw.memory_address);
		if (auto qualityString = paqw.ProductQualityToString(paqw.GetQuality()); !qualityString.IsNull())
		{
			productData.quality = qualityString.ToString();
		}
	};
	
	//Attributes ignored on purpose
	attributeHandler["ProductAttribute_TitleID_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData) {};
	attributeHandler["ProductAttribute_NoNotify_TA"] = [this](ProductAttributeWrapper paw, ProductStruct& productData) {};
}

void BetterInventoryExport::onUnload()
{
}



void BetterInventoryExport::OnInventDump(std::vector<std::string> params)
{
	if (params.size() < 2)
	{
		cvarManager->log("Invalid arguments! Usage: " + params.at(0) + " csv|json");
		return;
	}


	auto itemsWrapper = gameWrapper->GetItemsWrapper();
	auto inventoryUnlocked = itemsWrapper.GetCachedUnlockedProducts();
	auto inventory = itemsWrapper.GetOwnedProducts();

	std::vector<ProductStruct> products;

	for (auto unlockedProduct : inventoryUnlocked)
	{
		products.push_back(GetProductInfo(unlockedProduct));
	}
	for (auto unlockedProduct : inventory)
	{
		products.push_back(GetOnlineProductInfo(unlockedProduct));
	}

	//Remove any invalid products
	products.erase(std::remove_if(products.begin(), products.end(), [](const ProductStruct& ps) { return ps.product_id >= 0; }));

	std::string outputFormat = params.at(1);
	std::string destinationFile = "./bakkesmod/data/inventory." + outputFormat;
	if (outputFormat.compare("csv") == 0)
	{
		export_csv(destinationFile, products);
	}
	else if (outputFormat.compare("json") == 0)
	{
		export_json(destinationFile, products);
	}
	else
	{
		cvarManager->log("Output format not supported");
	}
}

ProductStruct BetterInventoryExport::GetProductInfo(ProductWrapper& product)
{
	ProductStruct prodData;
	prodData.product_id = product.GetID();

	if (auto longLabel = product.GetLongLabel(); !longLabel.IsNull())
	{
		prodData.name = longLabel.ToString();
	}
	if (auto slot = product.GetDisplayLabelSlot(); !slot.IsNull())
	{
		prodData.slot = slot.ToString();
	}

	prodData.instance_id = 0; //instance ID isn't exposed by SDK

	//Set quality before attributes, since there might be a quality override (blueprints)
	uint8_t quality = product.GetQuality();
	if (auto qualityInfo = productQualities.find(quality); qualityInfo != productQualities.end())
	{
		prodData.quality = qualityInfo->second.name;
	}
	return prodData;
}

ProductStruct BetterInventoryExport::GetOnlineProductInfo(OnlineProductWrapper& unlockedProduct)
{
	
	auto product = unlockedProduct.GetProduct();
	if (product.memory_address == NULL)
	{
		cvarManager->log("ERR? Invalid product " + std::to_string(unlockedProduct.GetProductID()));
		return { -1 };
	}
	ProductStruct prodData = GetProductInfo(product);

	for (auto attribute : unlockedProduct.GetAttributes())
	{
		std::string attributeType = attribute.GetAttributeType();

		if (auto attr = attributeHandler.find(attributeType); attr != attributeHandler.end())
		{
			attr->second(attribute, prodData);
		}
		else
		{
			cvarManager->log("Could not find handler for " + attributeType);
		}
	}

	prodData.tradeable = !unlockedProduct.GetIsUntradable();
	return prodData;
}

void BetterInventoryExport::export_csv(std::filesystem::path filename, std::vector<ProductStruct>& psv)
{
	std::ofstream myfile;
	myfile.open(filename);
	myfile << "product id,name,slot,paint,certification,certification value,certification label,quality,crate,tradeable,amount,instanceid,specialedition,blueprint item id, blueprint item, blueprint cost" << std::endl;

	for (auto ps : psv)
	{
		myfile 
			<< ps.product_id << "," 
			<< ps.name << "," 
			<< ps.slot << "," 
			<< ps.paint << "," 
			<< ps.certification << "," 
			<< ps.certification_value 
			<< "," << ps.rank_label 
			<< "," << ps.quality 
			<< "," << ps.crate 
			<< "," << (ps.tradeable ? "true" : "false") 
			<< "," << ps.amount 
			<< "," << ps.instance_id 
			<< "," << ps.special_edition 
			<< "," << ps.blueprint_item_id
			<< "," << ps.blueprint_item
			<< "," << ps.blueprint_cost
			<< "\n";
	}
}
#define EXP_VAL(name, val) (myfile << "\"" << name <<  "\": " << val << "," << std::endl)
#define EXP_STRING(name, val) EXP_VAL(name, "\"" << val << "\"")

void BetterInventoryExport::export_json(std::filesystem::path filename, std::vector<ProductStruct>& psv)
{
	std::ofstream myfile;
	myfile.open(filename);
	//myfile << "product id,name,slot,paint,certification,certification value,certification label,quality,crate,tradeable" << std::endl;

	myfile << "{\"inventory\": [";


	for (auto it = psv.begin(); it != psv.end(); it++)
	{
		myfile << "{" << std::endl;
		auto ps = *it;
		EXP_VAL("product_id", ps.product_id);
		EXP_STRING("name", ps.name);
		EXP_STRING("slot", ps.slot);
		EXP_STRING("paint", ps.paint);
		EXP_STRING("certification", ps.certification);
		EXP_VAL("certification_value", ps.certification_value);
		EXP_STRING("rank_label", ps.rank_label);
		EXP_STRING("quality", ps.quality);
		EXP_STRING("crate", ps.crate);
		EXP_VAL("amount", ps.amount);
		EXP_VAL("instance_id", std::to_string(ps.instance_id));
		EXP_STRING("special_edition", ps.special_edition);
		EXP_VAL("blueprint_item_id", ps.blueprint_item_id);
		EXP_STRING("blueprint_item", ps.blueprint_item);
		EXP_VAL("blueprint_cost", ps.blueprint_cost);

		//Doing this one manually, since we dont want to write a comma here!!
		myfile << "\"tradeable\": \"" << (ps.tradeable ? "true" : "false") << "\"";

		myfile << std::endl;
		
		myfile << "}";
		if (std::next(it) != psv.end())
			myfile << ",";
		myfile << "" << std::endl;
			}
	myfile << "]}";
}
#undef EXP_VAL
#undef EXP_STRING