/*
 * Defines structs used to load the elu xml's
 */
#pragma once


#include "../defs.h"

namespace rsx {

#define FLAG_DIFFUSE	(1)
#define FLAG_NORMAL		(1 << 1)
#define FLAG_SPECULAR	(1 << 2)
#define FLAG_OPACITY	(1 << 3)
#define FLAG_SELFILLUM	(1 << 4)

/* elu.xml tags */
#define MATERIAL_LIST_TAG	"MATERIALLIST"
#define MATERIAL_TAG		"MATERIAL"

#define MAT_DIFFUSE_TAG		"DIFFUSE"
#define MAT_AMBIENT_TAG		"AMBIENT"
#define MAT_SPECULAR_TAG	"SPECULAR"

#define MAT_SPECULAR_LEVEL_TAG	"SPECULAR_LEVEL"
#define MAT_GLOSSINESS_TAG		"GLOSSINESS"

#define MAT_TEXTURELIST_TAG		"TEXTURELIST"
#define MAT_TEXTURELAYER_TAG	"TEXTURELAYER"
#define MAT_DIFFUSEMAP_TAG		"DIFFUSEMAP"
#define MAT_NORMALMAP_TAG		"NORMALMAP"
#define MAT_SPECULARMAP_TAG		"SPECULARMAP"
#define MAT_OPACITYMAP_TAG		"OPACITYMAP"
#define MAT_SELFILLUMINATIONMAP_TAG	"SELFILLUMINATIONMAP"

#define MAT_ATTRIB_NAME		"name"

/* scene.xml tags */
#define SCENE_TAG		"SCENE"
#define SCENE_INST_TAG	"SCENEINSTANCE"
#define SCENE_COMMON_TAG	"COMMON"
#define SCENE_ATTRIB_NAME	"name"
#define	SCENE_POSITION_TAG	"POSITION"
#define SCENE_DIRECTION_TAG	"DIRECTION"
#define SCENE_UP_TAG		"UP"
#define SCENE_SCALE_TAG		"SCALE"
#define SCENE_PROPERTY_TAG	"PROPERTY"
#define SCENE_FILENAME_TAG	"FILENAME"

#define SCENE_LIGHT_TAG		"LIGHT"
#define SCENE_COLOR_TAG		"COLOR"
#define SCENE_INTENSITY_TAG			"INTENSITY"
#define SCENE_ATTENUATIONEND_TAG		"ATTENUATIONEND"
#define SCENE_ATTENUATIONSTART_TAG		"ATTENUATIONSTART"

#define PROP_SCENE_OBJECT_TAG	"SCENEOBJECT"
#define PROP_OBJECT_TAG			"OBJECT"
#define PROP_NAME_TAG			"NAME"
#define PROP_SCENE_FILE_TAG		"SceneFileName"
#define PROP_DEFINITION_TAG		"DEFINITION"


struct XMLActor
{
	bool isValid;
	std::string Name, eluName;
	XMLActor():isValid(false) {}
};


struct XMLMatreial
{
	float diffuse[3];
	float abient[3];
	float specular[3];
	float SpecularLevel;
	float glossines;

	std::string Name,
		        DiffuseMap,
		        NormalMap,
				SpecularMap,
				OpacityMap,
				SelfIlluminationMap;
	unsigned char Flag;
	void Clear()
	{
		Name.clear();
		DiffuseMap.clear();
		NormalMap.clear();
		SpecularMap.clear();
		OpacityMap.clear();
		SelfIlluminationMap.clear();
		Flag = 0;
		SpecularLevel = 0.0f;
		glossines = 0.0f;
		diffuse[2] = diffuse[1] = diffuse[0] = 0.0f;
		abient[2] = abient[1] = abient[0] = 0.0f;
		specular[2] = specular[1] = specular[0] = 0.0f;
	}
};

struct XMLObject
{
	float Position[3];
	float Dir[3];
	float Up[3];
	float Scale[3];
	bool isDynamic;

	std::string Name,
				SceneXMLFile;
	void Clear()
	{
		Name.clear();
		SceneXMLFile.clear();
		Position[2] = Position[1] = Position[0] = 0.0f;
		Dir[0] = 1.0f; Dir[1] = 0.0f; Dir[2] = 0.0f;
		Up[0] = 0.0f; Up[1] = 0.0f; Up[2] = 1.0f;
		Scale[0] = Scale[1] = Scale[2] = 1.0f;
	}
};

struct XMLLight
{
	float Position[3];
	float Dir[3];
	float Up[3];
	float Scale[3];

	float Intensity;
	float AttEnd;
	float AttStart;

	float Diffuse[3];

	std::string Name;

	void Clear()
	{
		Name.clear();
	}
};

}