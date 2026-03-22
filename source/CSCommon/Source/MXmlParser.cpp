#include "stdafx.h"
#include "MXmlParser.h"
#include "MZFileSystem.h"
#include "MXml.h"

#include "../FileSystem/FileSystem.h"

bool MXmlParser::ReadXml(const char* szFileName)
{
	MXmlDocument xmlIniData;

	xmlIniData.Create();

	if (!xmlIniData.LoadFromFile(szFileName)) {
		xmlIniData.Destroy();
		return false;
	}

	MXmlElement rootElement, chrElement, attrElement;

	char szTagName[256];

	rootElement = xmlIniData.GetDocumentElement();

	int iCount = rootElement.GetChildNodeCount();

	for (int i = 0; i < iCount; i++) {

		chrElement = rootElement.GetChildNode(i);
		chrElement.GetTagName(szTagName);

		if (szTagName[0] == '#') continue;

		ParseRoot(szTagName, &chrElement);
	}

	xmlIniData.Destroy();
	return true;
}

bool MXmlParser::ReadXml(MZFileSystem* pFileSystem, const char* szFileName)
{

	MXmlDocument	xmlIniData;
	xmlIniData.Create();

	std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(getFileManager()->open(szFileName));
    if (!pFile.get())
    {
        xmlIniData.Destroy();
        return false;
    }

    std::unique_ptr<char> buffer = std::unique_ptr<char>(new char[pFile->getSize()+1]);
    pFile->read(pFile->getSize(), buffer.get());
	buffer.get()[pFile->getSize()] = 0;

	if(!xmlIniData.LoadFromMemory(buffer.get()))
	{
		xmlIniData.Destroy();
		return false;
	}
    pFile.reset();


	MXmlElement rootElement, chrElement, attrElement;
	char szTagName[256];

	rootElement = xmlIniData.GetDocumentElement();
	int iCount = rootElement.GetChildNodeCount();

	for (int i = 0; i < iCount; i++) {

		chrElement = rootElement.GetChildNode(i);
		chrElement.GetTagName(szTagName);

		if (szTagName[0] == '#') continue;

		ParseRoot(szTagName, &chrElement);
	}

	xmlIniData.Destroy();

	return true;
}
