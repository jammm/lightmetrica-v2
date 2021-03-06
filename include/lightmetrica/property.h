/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/component.h>
#include <lightmetrica/math.h>
#include <sstream>
#include <vector>

LM_NAMESPACE_BEGIN

class PropertyTree;

//! Type of the property node
enum class PropertyNodeType : int
{
    Null,
    Scalar,         //!< Scalar type
    Sequence,       //!< Sequence type
    Map,            //!< Map type
    Undefined,
};

//! An element of the property
class PropertyNode : public Component
{
public:

    LM_INTERFACE_CLASS(PropertyNode, Component, 9);

public:

    PropertyNode() = default;
    LM_DISABLE_COPY_AND_MOVE(PropertyNode);

public:

    //! Get the tree associated to the node
    LM_INTERFACE_F(0, Tree, const PropertyTree*());

    //! Get the current node type
    LM_INTERFACE_F(1, Type, PropertyNodeType());

    //! Get the current line in the loaded property node
    LM_INTERFACE_F(2, Line, int());

    //! Key of the node
    LM_INTERFACE_F(3, Key, std::string());

    //! Scalar value of the node (raw version)
    LM_INTERFACE_F(4, RawScalar, const char*());

    //! Get a number of child elements
    LM_INTERFACE_F(5, Size, int());

    //! Find a child by name
    LM_INTERFACE_F(6, Child, const PropertyNode*(const std::string&));

    //! Get a child by index
    LM_INTERFACE_F(7, At, const PropertyNode*(int));

    //! Parent node (nullptr for root node)
    LM_INTERFACE_F(8, Parent, const PropertyNode*());

public:

    #pragma region Type conversion functions

    template <typename T>
    auto ChildAs(const std::string& name, const T& def) const noexcept -> T
    {
        LM_LOG_INFO("Loading parameter '" + name + "'");
        LM_LOG_INDENTER();
        const auto* child = Child(name);
        T ret;
        if (!child)
        {
            LM_LOG_WARN("Missing '" + name + "' element; using default value");
            return def;
        }
        if (!child->As<T>(ret))
        {
            LM_LOG_WARN("Failed to load '" + name + "' element; using default value");
            return def;
        }
        return ret;
    };

    template <typename T>
    auto ChildAs(const std::string& name, T& ret) const noexcept -> bool
    {
        LM_LOG_INFO("Loading parameter '" + name + "'");
        LM_LOG_INDENTER();
        const auto* child = Child(name);
        if (!child)
        {
            LM_LOG_WARN("Missing '" + name + "' element");
            return false;
        }
        if (!child->As<T>(ret))
        {
            LM_LOG_WARN("Failed to load '" + name + "' element");
            return false;
        }
        return true;
    }

    template <typename T>
    auto As(T& ret) const noexcept -> bool
    {
        try
        {
            ret = As<T>();
        }
        catch (const std::invalid_argument& e)
        {
            LM_LOG_ERROR("Invalid parameter: " + std::string(e.what()));
            return false;
        }
        return true;
    }

    template <typename T> auto As() const -> T;

    #pragma endregion

};

template <> inline auto PropertyNode::As<const char*>() const -> const char* { return RawScalar(); }
template <> inline auto PropertyNode::As<std::string>() const -> std::string { return RawScalar(); }
template <> inline auto PropertyNode::As<int>() const -> int { return std::stoi(RawScalar()); }
template <> inline auto PropertyNode::As<long long>() const -> long long { return std::stoll(RawScalar()); }
template <> inline auto PropertyNode::As<double>() const -> double { return std::stod(RawScalar()); }
template <> inline auto PropertyNode::As<float>() const -> float { return std::stof(RawScalar()); }

template <>
inline auto PropertyNode::As<Vec3>() const -> Vec3
{
    Vec3 v;
    std::stringstream ss(RawScalar());
    double t;
    int i = 0;
    while (ss >> t) { v[i++] = Float(t); }
    return v;
}

template <>
inline auto PropertyNode::As<Vec4>() const -> Vec4
{
    Vec4 v;
    std::stringstream ss(RawScalar());
    double t;
    int i = 0;
    while (ss >> t) { v[i++] = Float(t); }
    return v;
}

template <>
inline auto PropertyNode::As<Mat3>() const -> Mat3
{
    Mat3 m;
    std::stringstream ss(RawScalar());
    double t;
    int i = 0;
    while (ss >> t) { m[i/3][i%3] = Float(t); i++; }
    return m;
}

template <>
inline auto PropertyNode::As<Mat4>() const -> Mat4
{
    Mat4 m;
    std::stringstream ss(RawScalar());
    double t;
    int i = 0;
    while (ss >> t) { m[i/4][i%4] = Float(t); i++; }
    return m;
}

template <>
inline auto PropertyNode::As<std::vector<Float>>() const -> std::vector<Float>
{
    std::vector<Float> v;
    std::stringstream ss(RawScalar());
    double t;
    while (ss >> t) { v.push_back(Float(t)); }
    return v;
}

template <>
inline auto PropertyNode::As<std::vector<unsigned int>>() const -> std::vector<unsigned int>
{
    std::vector<unsigned int> v;
    std::stringstream ss(RawScalar());
    unsigned int t;
    while (ss >> t) { v.push_back(t); }
    return v;
}

/*!
    \brief Property tree.
    
    Manages tree structure.
    Mainly utilized as asset parameters.
    This class manages all instances of the property nodes.
*/
class PropertyTree : public Component
{
public:

    LM_INTERFACE_CLASS(PropertyTree, Component, 7);

public:

    PropertyTree() = default;
    LM_DISABLE_COPY_AND_MOVE(PropertyTree);

public:
    
    //! Load property tree from the file
    LM_INTERFACE_F(0, LoadFromFile, bool(const std::string&));

    //! Load property tree from YAML sequences
    LM_INTERFACE_F(1, LoadFromString, bool(const std::string&));

    //! Load property tree from YAML sequences with filename
    LM_INTERFACE_F(2, LoadFromStringWithFilename, bool(const std::string& input, const std::string& path, const std::string& basepath));

    //! Returns file path if the tree loaded from the file, otherwise returns empty string
    LM_INTERFACE_F(3, Path, std::string());

    //! Returns the base path of the asset loading
    LM_INTERFACE_F(4, BasePath, std::string());

    //! Get root node
    LM_INTERFACE_F(5, Root, const PropertyNode*());

    //! Returns loaded file content
    LM_INTERFACE_F(6, RawInput, std::string());

};

LM_NAMESPACE_END
