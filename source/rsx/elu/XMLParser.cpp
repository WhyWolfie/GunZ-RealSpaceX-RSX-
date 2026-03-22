#include "../defs.h"
#include "../file.h"
#include "XMLParser.h"
#include "RapidXML.h"

#include <iostream>

#include <memory>

using namespace rapidxml;

namespace rsx {

#define LOG(...)

#define RET_CLEAR_ON_NULL_PTR(p, ptr)		if (!(p)) { if (ptr) {delete ptr;} return false;}
#define UNPACK_BYTE2FLOAT(value, index) ((((value) >> index*8)& 0xFF) / 255.0f)

IFileManager * XMLParser::m_pFileMgr = nullptr;

bool XMLParser::parseXMLMaterial(const char * name, std::vector<XMLMatreial> &Ret)
{
    if (!m_pFileMgr) return false;
	std::unique_ptr<IFile> file = std::unique_ptr<IFile>(m_pFileMgr->open(name, FF_READ));

	if (file.get() == nullptr || !file->isOpen())
	{
		LOG("[XMLParser] Failed to open file %s\n", FileName);
		return false;
	}
	unsigned int size = file->getSize();
	char * buffer = new char[size+1];
	file->read(size, buffer);
	buffer[size] = 0;

	xml_document<> doc;

	doc.parse<0>(buffer);

	xml_node<> *MatList =  doc.first_node("XML")->first_node(MATERIAL_LIST_TAG);


	RET_CLEAR_ON_NULL_PTR(MatList, buffer);

	xml_node<> *CurMaterial = MatList->first_node(MATERIAL_TAG);
	XMLMatreial tmp;
	while(CurMaterial)
	{
		unsigned int color = 0;
		tmp.Clear();
		tmp.Name =  CurMaterial->first_attribute("name")->value();

		xml_node<> *MatProp = CurMaterial->first_node(MAT_DIFFUSE_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		color = toUShort3(MatProp->value());
		tmp.diffuse[0] = UNPACK_BYTE2FLOAT(color, 0);
		tmp.diffuse[1] = UNPACK_BYTE2FLOAT(color, 1);
		tmp.diffuse[2] = UNPACK_BYTE2FLOAT(color, 2);


		MatProp = MatProp->next_sibling(MAT_AMBIENT_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		color = toUShort3(MatProp->value());
		tmp.abient[0] = UNPACK_BYTE2FLOAT(color, 0);
		tmp.abient[1] = UNPACK_BYTE2FLOAT(color, 1);
		tmp.abient[2] = UNPACK_BYTE2FLOAT(color, 2);


		MatProp = MatProp->next_sibling(MAT_SPECULAR_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		color = toUShort3(MatProp->value());
		tmp.specular[0] = UNPACK_BYTE2FLOAT(color, 0);
		tmp.specular[1] = UNPACK_BYTE2FLOAT(color, 1);
		tmp.specular[2] = UNPACK_BYTE2FLOAT(color, 2);


		MatProp = MatProp->next_sibling(MAT_SPECULAR_LEVEL_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		tmp.SpecularLevel = toFloat(MatProp->value());


		MatProp = MatProp->next_sibling(MAT_GLOSSINESS_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		tmp.glossines = toFloat(MatProp->value());


		MatProp = MatProp->next_sibling(MAT_TEXTURELIST_TAG);
		RET_CLEAR_ON_NULL_PTR(MatProp, buffer);
		xml_node<> *TexLayer = MatProp->first_node(MAT_TEXTURELAYER_TAG);
		while (TexLayer)
		{
			xml_node<> *MapName = TexLayer->first_node(MAT_DIFFUSEMAP_TAG);
			if (MapName)
			{
				tmp.DiffuseMap = MapName->value();
				tmp.Flag |= FLAG_DIFFUSE;
			}
			MapName = TexLayer->first_node(MAT_NORMALMAP_TAG);
			if (MapName)
			{
				tmp.NormalMap = MapName->value();
				tmp.Flag |= FLAG_NORMAL;
			}
			MapName = TexLayer->first_node(MAT_SPECULARMAP_TAG);
			if (MapName)
			{
				tmp.SpecularMap = MapName->value();
				tmp.Flag |= FLAG_SPECULAR;
			}
			MapName = TexLayer->first_node(MAT_SELFILLUMINATIONMAP_TAG);
			if (MapName)
			{
				tmp.SelfIlluminationMap = MapName->value();
				tmp.Flag |= FLAG_SELFILLUM;
			}
			MapName = TexLayer->first_node(MAT_OPACITYMAP_TAG);
			if (MapName)
			{
				tmp.OpacityMap = MapName->value();
				tmp.Flag |= FLAG_OPACITY;
			}

			TexLayer = TexLayer->next_sibling(MAT_TEXTURELAYER_TAG);
		}


		Ret.push_back(tmp);
		CurMaterial = CurMaterial->next_sibling(MATERIAL_TAG);
	}

	SAFE_DELETE(buffer);
	return true;

}

bool XMLParser::parseScene(const char *name, XMLActor &actor, std::vector<XMLObject> &Ret, std::vector<XMLLight> *Ret2)
{
    if (!m_pFileMgr) return false;
	std::unique_ptr<IFile> file = std::unique_ptr<IFile>(m_pFileMgr->open(name, FF_READ));

	if (file.get() == nullptr  || !file->isOpen())
	{
		LOG("[XMLParser] Failed to open file %s\n", FileName);
		return false;
	}
	unsigned int size = file->getSize();
	char * buffer = new char[size+1];
	file->read(size, buffer);
	buffer[size] = 0;

	xml_document<> doc;

	doc.parse<0>(buffer);

	xml_node<> *Scene =  doc.first_node("ROOT")->first_node("SCENE");
	RET_CLEAR_ON_NULL_PTR(Scene, buffer)

	xml_node<> *Actor = Scene->first_node("ACTOR");
	if (Actor)
	{
		xml_node<> *Comm = Actor->first_node("COMMON");
		actor.Name = Comm->first_attribute("name")->value();
		xml_node<> *Prop = Actor->first_node("PROPERTY");
		actor.eluName = Prop->first_node("FILENAME")->value();
		actor.isValid = true;
	}

	xml_node<> *Object = Scene->first_node();

	XMLObject tmp;
	XMLLight tmp2;
	while (Object)
	{
		if (strcmp(Object->name(), SCENE_INST_TAG) == 0)
		{
			tmp.Clear();
			xml_node<> * Common = Object->first_node(SCENE_COMMON_TAG);
			RET_CLEAR_ON_NULL_PTR(Common, buffer);
			tmp.Name = Common->first_attribute(SCENE_ATTRIB_NAME)->value();

			xml_node<> *cProp = Common->first_node(SCENE_POSITION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp.Position);

			cProp = Common->first_node(SCENE_DIRECTION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp.Dir);

			cProp = Common->first_node(SCENE_SCALE_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp.Scale);

			cProp = Common->first_node(SCENE_UP_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp.Up);

			xml_node<> *Prop = Object->first_node(SCENE_PROPERTY_TAG);
			RET_CLEAR_ON_NULL_PTR(Prop, buffer);
			Prop = Prop->first_node(SCENE_FILENAME_TAG);
			RET_CLEAR_ON_NULL_PTR(Prop, buffer);
			tmp.SceneXMLFile = Prop->value();

			tmp.isDynamic = false;

			Ret.push_back(tmp);
		}
		else if (strcmp(Object->name(), SCENE_LIGHT_TAG) == 0 && Ret2 != NULL)
		{
			tmp2.Clear();
			xml_node<> * Common = Object->first_node(SCENE_COMMON_TAG);
			RET_CLEAR_ON_NULL_PTR(Common, buffer);
			tmp2.Name = Common->first_attribute(SCENE_ATTRIB_NAME)->value();

			xml_node<> *cProp = Common->first_node(SCENE_POSITION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp2.Position);

			cProp = Common->first_node(SCENE_DIRECTION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp2.Dir);

			cProp = Common->first_node(SCENE_UP_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp2.Up);

			cProp = Common->first_node(SCENE_SCALE_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp2.Scale);

			xml_node<> *Prop = Object->first_node(SCENE_PROPERTY_TAG);
			RET_CLEAR_ON_NULL_PTR(Prop, buffer);

			cProp = Prop->first_node(SCENE_COLOR_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			toFloat3(cProp->value(), tmp2.Diffuse);

			cProp = Prop->first_node(SCENE_INTENSITY_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp2.Intensity = toFloat(cProp->value());

			cProp = Prop->first_node(SCENE_ATTENUATIONEND_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp2.AttEnd = toFloat(cProp->value());

			cProp = Prop->first_node(SCENE_ATTENUATIONSTART_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp2.AttStart = toFloat(cProp->value());

			Ret2->push_back(tmp2);
		}

		Object = Object->next_sibling();
	}

	SAFE_DELETE(buffer);
	return true;
}

bool XMLParser::parseProp(const char *name, std::vector<XMLObject> &Ret)
{
    if (!m_pFileMgr) return false;
	std::unique_ptr<IFile> file = std::unique_ptr<IFile>(m_pFileMgr->open(name, FF_READ));

	if (file.get() == nullptr || !file->isOpen())
	{
		LOG("[XMLParser] Failed to open file %s\n", FileName);
		return false;
	}
	unsigned int size = file->getSize();
	char * buffer = new char[size+1];
	file->read(size, buffer);
	buffer[size] = 0;

	xml_document<> doc;

	doc.parse<0>(buffer);

	xml_node<> *Root =  doc.first_node("XML");
	RET_CLEAR_ON_NULL_PTR(Root, buffer);

	xml_node<> *Object = Root->first_node();

	XMLObject tmp;
	while (Object)
	{
		if (strcmp(Object->name(), PROP_SCENE_OBJECT_TAG) == 0 || strcmp(Object->name(), PROP_OBJECT_TAG) == 0)
		{
			tmp.Clear();
			xml_node<> * Common = Object->first_node(SCENE_COMMON_TAG);
			RET_CLEAR_ON_NULL_PTR(Common, buffer);

			xml_node<> *cProp = Common->first_node(SCENE_POSITION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp.Position[0] = toFloat(cProp->first_attribute("x")->value());
			tmp.Position[1] = toFloat(cProp->first_attribute("y")->value());
			tmp.Position[2] = toFloat(cProp->first_attribute("z")->value());

			cProp = Common->first_node(SCENE_DIRECTION_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp.Dir[0] = toFloat(cProp->first_attribute("x")->value());
			tmp.Dir[1] = toFloat(cProp->first_attribute("y")->value());
			tmp.Dir[2] = toFloat(cProp->first_attribute("z")->value());

			cProp = Common->first_node(SCENE_UP_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp.Up[0] = toFloat(cProp->first_attribute("x")->value());
			tmp.Up[1] = toFloat(cProp->first_attribute("y")->value());
			tmp.Up[2] = toFloat(cProp->first_attribute("z")->value());

			xml_node<> *Prop = Object->first_node(SCENE_PROPERTY_TAG);
			RET_CLEAR_ON_NULL_PTR(Prop, buffer);

			cProp = Prop->first_node(PROP_NAME_TAG);
			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp.Name = cProp->value();

			cProp = Prop->first_node(PROP_SCENE_FILE_TAG);
			bool isScene = true;
			if (!cProp)
			{
				cProp = Prop->first_node(PROP_DEFINITION_TAG);
				isScene = false;
				tmp.isDynamic = true;
			}

			RET_CLEAR_ON_NULL_PTR(cProp, buffer);
			tmp.SceneXMLFile = cProp->value();
			if (!isScene) tmp.SceneXMLFile.append(".scene.xml");

			Ret.push_back(tmp);
		}


		Object = Object->next_sibling();
	}
	SAFE_DELETE(buffer);
	return true;
}


float XMLParser::toFloat(const char *str)
{
	float r = 0;
	sscanf(str, "%f", &r);
	return r;
}

int XMLParser::toInt(const char *str)
{
	int i = 0;
	sscanf(str, "%d", &i);
	return i;
}

unsigned int XMLParser::toUShort3(const char *str)
{
	int a, b, c;
	a = b = c = 0;

	sscanf(str, "%i %i %i", &a, &b, &c);
	unsigned int ret = (a & 0xFF) + ((b & 0xFF) << 8) + ((c & 0xFF) << 16);
	return ret;
}

bool XMLParser::toFloat3(const char *str, float *fv)
{
	fv[0] = fv[1] = fv[2] = 0.0f;

	sscanf(str, "%f %f %f", &fv[0], &fv[1], &fv[2]);
	return true;
}

}