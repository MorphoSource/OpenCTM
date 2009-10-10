//-----------------------------------------------------------------------------
// Product:     OpenCTM tools
// File:        dae.cpp
// Description: Implementation of the DAE (Collada) file format
//              importer/exporter.
//-----------------------------------------------------------------------------
// Copyright (c) 2009 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//     1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//     2. Altered source versions must be plainly marked as such, and must not
//     be misrepresented as being the original software.
//
//     3. This notice may not be removed or altered from any source
//     distribution.
//-----------------------------------------------------------------------------

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <map>
#include <tinyxml.h>
#include "dae.h"

using namespace std;

enum Axis
{
  X,Y,Z,S,T
};

class Source
{
public:
  Source() : stride(0), count(0), offset(0)
  {
  }
  
  Source(const Source& copy) : array(copy.array), stride(copy.stride), count(copy.count), offset(copy.offset), params(copy.params)
  {
  }
  
  vector<float> array;
  size_t stride, count, offset;
  vector<string> params;

};

class Indexes {
public:
	Indexes(size_t _vertIndex = 0, size_t _normalIndex = 0, size_t _texcoordIndex = 0) : vertIndex(_vertIndex), normalIndex(_normalIndex), texcoordIndex(_texcoordIndex) {
		
	}
	
	Indexes(const Indexes& copy) : vertIndex(copy.vertIndex), normalIndex(copy.normalIndex), texcoordIndex(copy.texcoordIndex) {
		
	}
	
	size_t vertIndex, normalIndex, texcoordIndex;
};

enum Semantic
{
  VERTEX,
  NORMAL,
  TEXCOORD,
  POSITIONS,
  UNKNOWN
};

struct Input
{
  string source;
  Semantic semantic;
  size_t offset;
};

Semantic ToSemantic(const string& semantic)
{
  if (semantic == "VERTEX")
    return VERTEX;
  else if (semantic == "NORMAL")
    return NORMAL;
  else if (semantic == "TEXCOORD")
    return TEXCOORD;
  else if (semantic == "POSITIONS")
    return POSITIONS;
  else
    return UNKNOWN;
}

void ReadIndexArray(TiXmlElement* p , vector<size_t>& array)
{
  istringstream strStream (p->GetText());
  char val[100];
  size_t value = 0;
  while (!strStream.eof()) {
    strStream >> val;
    value = atoi(val);
    array.push_back(value);
  }
}

void ReadInputs(TiXmlElement* rootElem,bool& hasVerts,bool& hasNormals,bool& hasTexcoords, string& vertSource,string& normalSource,string& texcoordSource, vector<Input>& inputs) {
	TiXmlHandle root(rootElem);
	for(TiXmlElement* inputElem = root.FirstChild( "input" ).ToElement();inputElem; inputElem = inputElem->NextSiblingElement())
	{
		if(string(inputElem->Value()) != "input")
			continue;
		//TiXmlHandle input(inputElem);
		inputs.push_back(Input());
		inputs.back().source = string(inputElem->Attribute("source")).substr(1);
		inputs.back().offset = atoi(inputElem->Attribute("offset"));
		inputs.back().semantic = ToSemantic(inputElem->Attribute("semantic"));
		switch(inputs.back().semantic)
		{
			case VERTEX:
                hasVerts = true;
                vertSource = inputs.back().source;
                break;
			case NORMAL:
                hasNormals = true;
                normalSource = inputs.back().source;
                break;
			case TEXCOORD:
                hasTexcoords = true;
                texcoordSource = inputs.back().source;
                break;
			default:
                break;
		}
	}
}

Source& GetSource(map<string, Source >& sources, map<string, vector<Input> >& vertices,const string& source)
{
  map<string, Source >::iterator srcIterator = sources.find(source);
  if (srcIterator != sources.end())
    return srcIterator->second;
  map<string, vector<Input> >::iterator vertIterator = vertices.find(source);
  if (vertIterator != vertices.end() ) {
    for (vector<Input>::iterator i = vertIterator->second.begin(); i != vertIterator->second.end() ; ++i) {
      srcIterator = sources.find(i->source);
      if (srcIterator != sources.end())
        return srcIterator->second;
      
    }
  } else {
    throw string("Error");
  }
  
  return srcIterator->second;
}

void InsertVertNormalTexcoord(vector<Vector3>& vertVector,vector<Vector3>& normalVector,vector<Vector2>& texcoordVector, bool hasVerts, bool hasNormals, bool hasTexcoords,const string& vertSource ,const string& normalSource ,const string& texcoordSource ,size_t vertIndex , size_t normalIndex , size_t texcoordIndex, map<string, Source >& sources,map<string, vector<Input> >& vertices)
{
  if (hasVerts) {
    Source& src = GetSource(sources, vertices , vertSource);
    float x = 0, y = 0, z = 0;
    if (src.stride >= 1)
      x = src.array[src.offset + vertIndex*src.stride];
    if (src.stride >= 2)
      y = src.array[src.offset + vertIndex*src.stride + 1];
    if (src.stride >= 3)
      z = src.array[src.offset + vertIndex*src.stride + 2];
    vertVector.push_back(Vector3(x,y,z));
  }
  
  if (hasNormals) {
    Source& src = GetSource(sources, vertices , normalSource);
    float x = 0, y = 0, z = 0;
    if (src.stride >= 1)
      x = src.array[src.offset + normalIndex*src.stride];
    if (src.stride >= 2)
      y = src.array[src.offset + normalIndex*src.stride + 1];
    if (src.stride >= 3)
      z = src.array[src.offset + normalIndex*src.stride + 2];
    normalVector.push_back(Vector3(x,y,z) );
  }
  
  if (hasTexcoords) {
    Source& src = GetSource(sources, vertices , texcoordSource);
    float s = 0, t = 0;
    if (src.stride >= 1)
      s = src.array[src.offset + texcoordIndex*src.stride];
    if (src.stride >= 2)
      t = src.array[src.offset + texcoordIndex*src.stride + 1];
    
    texcoordVector.push_back(Vector2(s,t));
  }
}

/// Import a DAE file from a file.
void Import_DAE(const char * aFileName, Mesh &aMesh)
{
  // Clear the mesh
  aMesh.Clear();
  TiXmlDocument doc(aFileName);
  
  if (doc.LoadFile())
  {
    
    TiXmlHandle hDoc(&doc);
    TiXmlElement* elem = hDoc.FirstChildElement().Element();
    TiXmlHandle hRoot(elem);
    
    map<string, Source > sources;
    size_t indicesOffset = 0, vertexOffset = 0, texcoordOffset = 0, normalOffset = 0;
    
    TiXmlHandle geometry = hRoot.FirstChild( "library_geometries" ).FirstChild("geometry");
    for(elem = geometry.ToElement(); elem; elem=elem->NextSiblingElement())
    {
      TiXmlHandle geometry(elem);
      
      TiXmlElement* meshElem =  geometry.FirstChild("mesh").ToElement();
      
      if(meshElem)
      {
        TiXmlHandle mesh(meshElem);
        
        TiXmlElement* sourceElem;
        for(sourceElem = mesh.FirstChild("source").ToElement(); sourceElem;
            sourceElem = sourceElem->NextSiblingElement())
        {
          if(string(sourceElem->Value()) != "source")
            continue;
          TiXmlHandle source(sourceElem);
          string id = source.ToElement()->Attribute("id");
          TiXmlElement* arr = sourceElem->FirstChild("float_array")->ToElement();
          string str = arr->GetText();
          istringstream strStream (str);
          sources.insert(make_pair(id, Source()));
          
          TiXmlElement* techniqueElem = sourceElem->FirstChild("technique_common")->ToElement();
          TiXmlElement* accessorElem = techniqueElem->FirstChild("accessor")->ToElement();
          
          sources[id].stride = atoi(accessorElem->Attribute("stride"));
          sources[id].count = atoi(accessorElem->Attribute("count"));
		  if (accessorElem->Attribute("offset"))
			  sources[id].offset = atoi(accessorElem->Attribute("offset"));
			
          char val[100];
          float value = 0;
          while(!strStream.eof())
          {
            strStream >> val;
            value = float(atof(val));
            sources[id].array.push_back(value);
          }
        }
        
        TiXmlElement* verticesElem = mesh.FirstChild("vertices").ToElement();
        map<string, vector<Input> > vertices;
        if (verticesElem) {
          string id = verticesElem->Attribute("id");
          vertices.insert(make_pair(id, vector<Input>()));
          TiXmlElement* inputElem;
          for(inputElem = verticesElem->FirstChild("input")->ToElement();
              inputElem; inputElem = inputElem->NextSiblingElement())
          {
            if(string(inputElem->Value()) != "input")
              continue;
            
            vertices[id].push_back(Input());
            vertices[id].back().source = string(inputElem->Attribute("source")).substr(1);
            vertices[id].back().semantic = ToSemantic(inputElem->Attribute("semantic"));
          }
        }
        
        TiXmlElement* trianglesElem = mesh.FirstChild("triangles").ToElement();
        if(trianglesElem)
        {
          TiXmlHandle triangles(trianglesElem);
          vector<Input> inputs;
          bool hasVerts = false, hasNormals = false, hasTexcoords = false;
          string vertSource = "", normalSource = "", texcoordSource = ""; 
			/*
          TiXmlElement* inputElem;
          for(inputElem = triangles.FirstChild( "input" ).ToElement();
              inputElem; inputElem = inputElem->NextSiblingElement())
          {
            if(string(inputElem->Value()) != "input")
              continue;
            //TiXmlHandle input(inputElem);
            inputs.push_back(Input());
            inputs.back().source = string(inputElem->Attribute("source")).substr(1);
            inputs.back().offset = atoi(inputElem->Attribute("offset"));
            inputs.back().semantic = ToSemantic(inputElem->Attribute("semantic"));
            switch(inputs.back().semantic)
            {
              case VERTEX:
                hasVerts = true;
                vertSource = inputs.back().source;
                break;
              case NORMAL:
                hasNormals = true;
                normalSource = inputs.back().source;
                break;
              case TEXCOORD:
                hasTexcoords = true;
                texcoordSource = inputs.back().source;
                break;
              default:
                break;
            }
          }
			*/
		  ReadInputs(trianglesElem, hasVerts, hasNormals, hasTexcoords, vertSource, normalSource, texcoordSource, inputs);
          
          vector<size_t> pArray;
          TiXmlElement* p = triangles.FirstChild( "p" ).ToElement();
          
          ReadIndexArray(p,pArray);
          
          vector<size_t> indexVector;
          vector<Vector3> vertVector, normalVector;
          vector<Vector2> texcoordVector;
          map<size_t, map<size_t, map< size_t, size_t > > > prevIndices;
          size_t index = 0;
          for (size_t i = 0; i < pArray.size() ; i += inputs.size()) {
            size_t vertIndex = 0, normalIndex = 0, texcoordIndex = 0;
            for (vector<Input>::const_iterator j = inputs.begin(); j != inputs.end(); ++j) {
              switch (j->semantic) {
                case VERTEX:
                  vertIndex = pArray[i + j->offset];
                  break;
                case NORMAL:
                  normalIndex = pArray[i + j->offset];
                  break;
                case TEXCOORD:
                  texcoordIndex = pArray[i + j->offset];
                  break;
                default:
                  break;
              }
            }
            map<size_t, map<size_t, map< size_t, size_t > > >::iterator prevIt1 = prevIndices.find(vertIndex);
            
            if(prevIt1 != prevIndices.end())
            {
              map<size_t, map< size_t, size_t > >::iterator prevIt2 = prevIt1->second.find(normalIndex);
              if(prevIt2 != prevIt1->second.end())
              {
                map< size_t, size_t >::iterator prevIt3 = prevIt2->second.find(texcoordIndex);
                if(prevIt3 != prevIt2->second.end())
                {
                  indexVector.push_back(prevIt3->second);
                }
                else
                {
                  indexVector.push_back(index);
                  prevIt2->second.insert(make_pair(texcoordIndex, index));
                  InsertVertNormalTexcoord(vertVector, normalVector, texcoordVector, hasVerts, hasNormals, hasTexcoords, vertSource, normalSource, texcoordSource, vertIndex, normalIndex, texcoordIndex, sources, vertices);
                  ++index;
                }
              }
              else
              {
                indexVector.push_back(index);
                prevIt1->second.insert(make_pair(normalIndex, map< size_t, size_t >()));
                prevIt1->second[normalIndex].insert(make_pair(texcoordIndex, index));
                InsertVertNormalTexcoord(vertVector, normalVector, texcoordVector, hasVerts, hasNormals, hasTexcoords, vertSource, normalSource, texcoordSource, vertIndex, normalIndex, texcoordIndex, sources, vertices);
                ++index;
              }
            }
            else
            {
              indexVector.push_back(index);
              prevIndices.insert(make_pair(vertIndex,map<size_t, map< size_t, size_t > >()));
              prevIndices[vertIndex].insert(make_pair(normalIndex, map< size_t, size_t >()));
              prevIndices[vertIndex][normalIndex].insert(make_pair(texcoordIndex, index));
              InsertVertNormalTexcoord(vertVector, normalVector, texcoordVector, hasVerts, hasNormals, hasTexcoords, vertSource, normalSource, texcoordSource, vertIndex, normalIndex, texcoordIndex, sources, vertices);
              ++index;
            }
            
          }
			
		  TiXmlElement* polylistElem = mesh.FirstChild("polylist").ToElement();
			
		  if (polylistElem) {
			  TiXmlHandle polylist(polylistElem);
			  vector<size_t> vcountArray, pArray;
			  TiXmlElement* vcountElem = polylist.FirstChild("vcount").ToElement();
			  ReadIndexArray(vcountElem, vcountArray);
			  TiXmlElement* pElem = polylist.FirstChild("p").ToElement();
			  ReadIndexArray(pElem, pArray);
			  vector<Input> inputs;
			  bool hasVerts = false, hasNormals = false, hasTexcoords = false;
			  string vertSource = "", normalSource = "", texcoordSource = ""; 
			  
			  ReadInputs(polylistElem, hasVerts, hasNormals, hasTexcoords, vertSource, normalSource, texcoordSource, inputs);
			  size_t offset = 0;
			  for (size_t i = 0; i < vcountArray.size(); ++i) {
				  vector<Indexes> convexPolygon;
				  for (size_t j = 0; j < vcountArray[i]; ++j) {
					  convexPolygon.push_back(Indexes());
					  for (vector<Input>::const_iterator j = inputs.begin(); j != inputs.end(); ++j) {
						  switch (j->semantic) {
							  case VERTEX:
								  convexPolygon.back().vertIndex = pArray[offset + j->offset];
								  break;
							  case NORMAL:
								  convexPolygon.back().normalIndex = pArray[offset + j->offset];
								  break;
							  case TEXCOORD:
								  convexPolygon.back().texcoordIndex = pArray[offset + j->offset];
								  break;
							  default:
								  break;
						  }
					  }
				  }
				  offset += vcountArray[i];
			  }
			  
			  
			  
		  }
			
          size_t indicesOff = indicesOffset, vertexOff = vertexOffset, normalOff = normalOffset, texcoordOff = texcoordOffset;
          indicesOffset += indexVector.size();
          vertexOffset += vertVector.size();
          normalOffset += normalVector.size();
          texcoordOffset += texcoordVector.size();
          aMesh.mIndices.resize(indicesOffset );
          aMesh.mVertices.resize(vertexOffset );
          aMesh.mNormals.resize(normalOffset );
          aMesh.mTexCoords.resize(texcoordOffset );
          
          for(size_t i = 0; i < indexVector.size(); ++i)
            aMesh.mIndices[indicesOff + i] = indexVector[i];
          
          for(size_t i = 0; i < vertVector.size(); ++i)
            aMesh.mVertices[vertexOff + i] = vertVector[i];
          
          for(size_t i = 0; i < normalVector.size(); ++i)
            aMesh.mNormals[normalOff + i] = normalVector[i];
          
          for(size_t i = 0; i < texcoordVector.size(); ++i)
            aMesh.mTexCoords[texcoordOff + i] = texcoordVector[i];
        }
      }
    }
  }
  else
    throw runtime_error("Could not open input file.");
}

/// Dump a float array to an XML text node.
static void FloatArrayToXML(TiXmlElement * aNode, float * aArray,
  unsigned int aCount)
{
  stringstream ss;
  for(unsigned int i = 0; i < aCount; ++ i)
    ss << aArray[i] << " ";
  aNode->LinkEndChild(new TiXmlText(ss.str().c_str()));
}

/// Export a DAE file to a file.
void Export_DAE(const char * aFileName, Mesh &aMesh)
{
  TiXmlDocument xmlDoc;
  TiXmlElement * elem;

  // Set XML declaration
  xmlDoc.LinkEndChild(new TiXmlDeclaration("1.0", "utf-8", ""));
 
  // Create root node
  TiXmlElement * root = new TiXmlElement("COLLADA");
  xmlDoc.LinkEndChild(root);
  root->SetAttribute("xmlns", "http://www.collada.org/2005/11/COLLADASchema");
  root->SetAttribute("version", "1.4.1");

  // Create traceability nodes
  TiXmlElement * asset = new TiXmlElement("asset");
  root->LinkEndChild(asset);
  TiXmlElement * contributor = new TiXmlElement("contributor");
  asset->LinkEndChild(contributor);
  TiXmlElement * authoring_tool = new TiXmlElement("authoring_tool");
  contributor->LinkEndChild(authoring_tool);
  authoring_tool->LinkEndChild(new TiXmlText("ctmconv"));
  TiXmlElement * comments = new TiXmlElement("comments");
  contributor->LinkEndChild(comments);
  comments->LinkEndChild(new TiXmlText(aMesh.mComment.c_str()));
  TiXmlElement * up_axis = new TiXmlElement("up_axis");
  asset->LinkEndChild(up_axis);
  up_axis->LinkEndChild(new TiXmlText("Z_UP"));

  // Create the geometry nodes
  TiXmlElement * library_geometries = new TiXmlElement("library_geometries");
  root->LinkEndChild(library_geometries);
  TiXmlElement * geometry = new TiXmlElement("geometry");
  library_geometries->LinkEndChild(geometry);
  geometry->SetAttribute("id", "Mesh-1");
  geometry->SetAttribute("name", "Mesh-1");
  TiXmlElement * mesh = new TiXmlElement("mesh");
  geometry->LinkEndChild(mesh);

  // Vertices (positions)
  TiXmlElement * source_position = new TiXmlElement("source");
  mesh->LinkEndChild(source_position);
  source_position->SetAttribute("id", "Mesh-1-positions");
  source_position->SetAttribute("name", "position");
  TiXmlElement * positions_array = new TiXmlElement("float_array");
  source_position->LinkEndChild(positions_array);
  positions_array->SetAttribute("id", "Mesh-1-positions-array");
  positions_array->SetAttribute("count", int(aMesh.mVertices.size() * 3));
  FloatArrayToXML(positions_array, &aMesh.mVertices[0].x, aMesh.mVertices.size() * 3);
  TiXmlElement * positions_technique = new TiXmlElement("technique_common");
  source_position->LinkEndChild(positions_technique);
  TiXmlElement * positions_technique_accessor = new TiXmlElement("accessor");
  positions_technique->LinkEndChild(positions_technique_accessor);
  positions_technique_accessor->SetAttribute("count", int(aMesh.mVertices.size()));
  positions_technique_accessor->SetAttribute("offset", 0);
  positions_technique_accessor->SetAttribute("source", "#Mesh-1-positions-array");
  positions_technique_accessor->SetAttribute("stride", 3);
  elem = new TiXmlElement("param");
  positions_technique_accessor->LinkEndChild(elem);
  elem->SetAttribute("name", "X");
  elem->SetAttribute("type", "float");
  elem = new TiXmlElement("param");
  positions_technique_accessor->LinkEndChild(elem);
  elem->SetAttribute("name", "Y");
  elem->SetAttribute("type", "float");
  elem = new TiXmlElement("param");
  positions_technique_accessor->LinkEndChild(elem);
  elem->SetAttribute("name", "Z");
  elem->SetAttribute("type", "float");

  // Normals
  bool hasNormals = (aMesh.mNormals.size() == aMesh.mVertices.size());
  if(hasNormals)
  {
    TiXmlElement * source_normal = new TiXmlElement("source");
    mesh->LinkEndChild(source_normal);
    source_normal->SetAttribute("id", "Mesh-1-normals");
    source_normal->SetAttribute("name", "normal");
    TiXmlElement * normals_array = new TiXmlElement("float_array");
    source_normal->LinkEndChild(normals_array);
    normals_array->SetAttribute("id", "Mesh-1-normals-array");
    normals_array->SetAttribute("count", int(aMesh.mVertices.size() * 3));
    FloatArrayToXML(normals_array, &aMesh.mNormals[0].x, aMesh.mNormals.size() * 3);
    TiXmlElement * normals_technique = new TiXmlElement("technique_common");
    source_normal->LinkEndChild(normals_technique);
    TiXmlElement * normals_technique_accessor = new TiXmlElement("accessor");
    normals_technique->LinkEndChild(normals_technique_accessor);
    normals_technique_accessor->SetAttribute("count", int(aMesh.mVertices.size()));
    normals_technique_accessor->SetAttribute("offset", 0);
    normals_technique_accessor->SetAttribute("source", "#Mesh-1-normals-array");
    normals_technique_accessor->SetAttribute("stride", 3);
    elem = new TiXmlElement("param");
    normals_technique_accessor->LinkEndChild(elem);
    elem->SetAttribute("name", "X");
    elem->SetAttribute("type", "float");
    elem = new TiXmlElement("param");
    normals_technique_accessor->LinkEndChild(elem);
    elem->SetAttribute("name", "Y");
    elem->SetAttribute("type", "float");
    elem = new TiXmlElement("param");
    normals_technique_accessor->LinkEndChild(elem);
    elem->SetAttribute("name", "Z");
    elem->SetAttribute("type", "float");
  }

  // UV map
  bool hasTexCoords = (aMesh.mTexCoords.size() == aMesh.mVertices.size());
  if(hasTexCoords)
  {
    TiXmlElement * source_map1 = new TiXmlElement("source");
    mesh->LinkEndChild(source_map1);
    source_map1->SetAttribute("id", "Mesh-1-map1");
    source_map1->SetAttribute("name", "map1");
    TiXmlElement * map1_array = new TiXmlElement("float_array");
    source_map1->LinkEndChild(map1_array);
    map1_array->SetAttribute("id", "Mesh-1-map1-array");
    map1_array->SetAttribute("count", int(aMesh.mVertices.size() * 3));
    FloatArrayToXML(map1_array, &aMesh.mTexCoords[0].u, aMesh.mTexCoords.size() * 2);
    TiXmlElement * map1_technique = new TiXmlElement("technique_common");
    source_map1->LinkEndChild(map1_technique);
    TiXmlElement * map1_technique_accessor = new TiXmlElement("accessor");
    map1_technique->LinkEndChild(map1_technique_accessor);
    map1_technique_accessor->SetAttribute("count", int(aMesh.mVertices.size()));
    map1_technique_accessor->SetAttribute("offset", 0);
    map1_technique_accessor->SetAttribute("source", "#Mesh-1-map1-array");
    map1_technique_accessor->SetAttribute("stride", 2);
    elem = new TiXmlElement("param");
    map1_technique_accessor->LinkEndChild(elem);
    elem->SetAttribute("name", "S");
    elem->SetAttribute("type", "float");
    elem = new TiXmlElement("param");
    map1_technique_accessor->LinkEndChild(elem);
    elem->SetAttribute("name", "T");
    elem->SetAttribute("type", "float");
  }

  // Vertices
  TiXmlElement * vertices = new TiXmlElement("vertices");
  mesh->LinkEndChild(vertices);
  vertices->SetAttribute("id", "Mesh-1-vertices");
  TiXmlElement * vertices_input = new TiXmlElement("input");
  vertices->LinkEndChild(vertices_input);
  vertices_input->SetAttribute("semantic", "POSITION");
  vertices_input->SetAttribute("source", "#Mesh-1-positions");


  // Triangles
  TiXmlElement * triangles = new TiXmlElement("triangles");
  mesh->LinkEndChild(triangles);
  triangles->SetAttribute("count", int(aMesh.mIndices.size() / 3));
  int triangleInputCount = 0;
  elem = new TiXmlElement("input");
  triangles->LinkEndChild(elem);
  elem->SetAttribute("offset", triangleInputCount);
  elem->SetAttribute("semantic", "VERTEX");
  elem->SetAttribute("source", "#Mesh-1-vertices");
  ++ triangleInputCount;
  if(hasNormals)
  {
    elem = new TiXmlElement("input");
    triangles->LinkEndChild(elem);
    elem->SetAttribute("offset", triangleInputCount);
    elem->SetAttribute("semantic", "NORMAL");
    elem->SetAttribute("source", "#Mesh-1-normals");
    ++ triangleInputCount;
  }
  if(hasTexCoords)
  {
    elem = new TiXmlElement("input");
    triangles->LinkEndChild(elem);
    elem->SetAttribute("offset", triangleInputCount);
    elem->SetAttribute("semantic", "TEXCOORD");
    elem->SetAttribute("source", "#Mesh-1-map1");
    elem->SetAttribute("set", 0);
    ++ triangleInputCount;
  }
  {
    elem = new TiXmlElement("p");
    triangles->LinkEndChild(elem);
    stringstream ss;
    for(unsigned int i = 0; i < aMesh.mIndices.size(); ++ i)
      for(int j = 0; j < triangleInputCount; ++ j)
        ss << aMesh.mIndices[i] << " ";
    elem->LinkEndChild(new TiXmlText(ss.str().c_str()));
  }

  // Save the XML document to a file
  xmlDoc.SaveFile(aFileName);
  if(xmlDoc.Error())
    throw runtime_error(string(xmlDoc.ErrorDesc()));
}
