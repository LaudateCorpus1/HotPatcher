#include "DependenciesParser/FDefaultAssetDependenciesParser.h"
#include "FlibAssetManageHelper.h"
#include "HotPatcherLog.h"


void FAssetDependenciesParser::Parse(const FAssetDependencies& ParseConfig)
{
	SCOPED_NAMED_EVENT_TCHAR(TEXT("FAssetDependenciesParser::Parse"),FColor::Red);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TSet<FName> AssetPackageNames;
	{
		SCOPED_NAMED_EVENT_TCHAR(TEXT("Get AssetPackageNames by filter"),FColor::Red);
		TArray<FAssetData> AssetDatas;
		UFlibAssetManageHelper::GetAssetsData(ParseConfig.IncludeFilters,AssetDatas);

		for(const auto& AssetData:AssetDatas)
		{
			bool bIsIgnore = false;
			for(const auto& IgnoreFilter:ParseConfig.IgnoreFilters)
			{
				if(AssetData.PackageName.ToString().StartsWith(IgnoreFilter))
				{
					bIsIgnore = true;
					break;
				}
			}
			if(!bIsIgnore)
			{
				AssetPackageNames.Add(AssetData.PackageName);
				if(ParseConfig.bRedirector && AssetData.IsRedirector())
				{
					if(!AssetData.PackageName.IsNone())
					{
						AssetPackageNames.Append(FAssetDependenciesParser::GatherAssetDependicesInfoRecursively(AssetRegistryModule,AssetData.PackageName,ParseConfig.AssetRegistryDependencyTypes,false,TSet<FName>{},ScanedCaches));
					}
				}
			}
		}
	}
	
	Results.Append(AssetPackageNames);
	
	{
		SCOPED_NAMED_EVENT_TCHAR(TEXT("Get dependencies for filters"),FColor::Red);
		for(FName PackageName:AssetPackageNames)
		{
			Results.Append(FAssetDependenciesParser::GatherAssetDependicesInfoRecursively(AssetRegistryModule,PackageName,ParseConfig.AssetRegistryDependencyTypes,true,TSet<FName>{},ScanedCaches));
		}
	}
	
	{
		SCOPED_NAMED_EVENT_TCHAR(TEXT("Get dependencies for SpecifyAsset"),FColor::Red);
		for(const auto& SpecifyAsset:ParseConfig.InIncludeSpecifyAsset)
		{
			if(!SpecifyAsset.Asset.IsValid())
			{
				continue;
			}
			Results.Add(FName(SpecifyAsset.Asset.GetLongPackageName()));
			if(SpecifyAsset.bAnalysisAssetDependencies)
			{
				Results.Append(FAssetDependenciesParser::GatherAssetDependicesInfoRecursively(AssetRegistryModule,FName(SpecifyAsset.Asset.GetLongPackageName()),SpecifyAsset.AssetRegistryDependencyTypes,SpecifyAsset.bAnalysisAssetDependencies,TSet<FName>{},ScanedCaches));
			}
		}
	}

	Results.Remove(FName(NAME_None));
}

bool IsValidPackageName(const FString& LongPackageName)
{
	SCOPED_NAMED_EVENT_TCHAR(TEXT("GatherAssetDependicesInfoRecursively"),FColor::Red);
	bool bStatus = false;
	if (!LongPackageName.IsEmpty() && !FPackageName::IsScriptPackage(LongPackageName) && !FPackageName::IsMemoryPackage(LongPackageName))
	{
		bStatus = true;
	}
	return bStatus;
}

TSet<FName> FAssetDependenciesParser::GatherAssetDependicesInfoRecursively(FAssetRegistryModule& InAssetRegistryModule,
	FName InLongPackageName, const TArray<EAssetRegistryDependencyTypeEx>& InAssetDependencyTypes,
	bool bRecursively, TSet<FName> IgnorePackageNames,FScanedCachesType& ScanedCaches)
{
	TSet<FName> AssetDependencies;
	SCOPED_NAMED_EVENT_TCHAR(TEXT("GatherAssetDependicesInfoRecursively"),FColor::Red);
	IgnorePackageNames.Add(InLongPackageName);
	
	// TArray<EAssetRegistryDependencyType::Type> AssetTypes;
	
	bool bGetDependenciesSuccess = false;
	EAssetRegistryDependencyType::Type TotalType = EAssetRegistryDependencyType::None;

	for (const auto& DepType : InAssetDependencyTypes)
	{
		TotalType = (EAssetRegistryDependencyType::Type)((uint8)TotalType | (uint8)UFlibAssetManageHelper::ConvAssetRegistryDependencyToInternal(DepType));
	}

	TArray<FName> CurrentAssetDependencies;
	
	// AssetTypes.AddUnique(UFlibAssetManageHelper::ConvAssetRegistryDependencyToInternal(DepType));
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bGetDependenciesSuccess = InAssetRegistryModule.Get().GetDependencies(InLongPackageName, CurrentAssetDependencies, TotalType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (!bGetDependenciesSuccess)
		return AssetDependencies;
	
	for (auto &LongPackageName : CurrentAssetDependencies)
	{
		// ignore /Script/WeatherSystem and self
		if(LongPackageName.IsNone() || !IsValidPackageName(LongPackageName.ToString()) || LongPackageName == InLongPackageName)
			continue;
		AssetDependencies.Add(LongPackageName);
	}

	if(bRecursively)
	{
		TSet<FName> Dependencies;
		for(const auto& AssetPackageName:AssetDependencies)
		{
			if(AssetPackageName.IsNone())
				continue;
			if(IgnorePackageNames.Contains(AssetPackageName))
				continue;
				
			if(!ScanedCaches.Contains(AssetPackageName))
			{
				ScanedCaches.Add(AssetPackageName,GatherAssetDependicesInfoRecursively(InAssetRegistryModule, AssetPackageName, InAssetDependencyTypes,true,IgnorePackageNames,ScanedCaches));
			}
			Dependencies.Append(*ScanedCaches.Find(AssetPackageName));
		}
		AssetDependencies.Append(Dependencies);
	}
	return AssetDependencies;
}
