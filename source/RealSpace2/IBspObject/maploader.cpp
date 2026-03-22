#include "stdafx.h"
#include "maploader.h"
#ifdef pi
#undef pi
#endif
#include "rapidxml\rapidxml.hpp"
#include "../../rsx/rsx.h"
#include <memory>
#include "rsxmap.h"

IBspObject * MapLoader::load(const char * name, const char * ext)
{
    std::string path = name;
    path.append(".").append(ext);
    std::unique_ptr<IFile> file = std::unique_ptr<IFile>(rsx::getEngine()->geFileManager()->open(path.c_str()));

    if (file.get() == nullptr || !file->isOpen())
        return nullptr;

    uint size = file->getSize();
    std::unique_ptr<char> data = std::unique_ptr<char>(new char[size + 1]);
    if (!file->read(size, data.get()) == size)
        return nullptr;

    data.get()[size] = 0;

    rapidxml::xml_document<> doc;
	doc.parse<0>(data.get());

    rapidxml::xml_node<> * node = doc.first_node("XML");
    if (node == nullptr)
        return nullptr;
    node = node->first_node("RSX");

    std::unique_ptr<IBspObject>  map;

    if (node == nullptr)
        map = std::unique_ptr<IBspObject>(new RBspObject());
    else
        map = std::unique_ptr<IBspObject>(new RSXMap());

    if (map->Open(name, ext))
        return map.release();
    else
        return nullptr;
}