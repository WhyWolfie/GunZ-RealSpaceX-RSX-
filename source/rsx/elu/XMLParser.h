/*
 * Class to load RS3 xml's
 */
#pragma once

#include "../defs.h"
#include "../interface.h"
#include "XMLFileStructs.h"

namespace rsx {

typedef std::vector<XMLMatreial> XMLMaterialVector;
typedef std::vector<XMLObject> XMLObjectVector;
typedef std::vector<XMLLight> XMLLightVector;

class XMLParser
{
	static float toFloat(const char *str);
	static int toInt(const char *str);
	static unsigned int toUShort3(const char *str);
	static bool toFloat3(const char *str, float *fv);
    
    static IFileManager * m_pFileMgr;

public:

    static void setFileManager(IFileManager * fm) {m_pFileMgr = fm;}

	/*
	 * Parses .elu.xml files
	 * @param FileName xml file name
	 * @param Ret reference to the XMLMaterialVector.
	 */
	static bool parseXMLMaterial(const char *FileName, XMLMaterialVector &Ret);

	static bool parseScene(const char *FileName, XMLActor &actor, XMLObjectVector &Ret, XMLLightVector *Ret2 = NULL);

	static bool parseProp(const char *FileName, XMLObjectVector &Ret);
};

}