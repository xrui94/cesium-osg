#pragma once

#include <glm/glm.hpp>

#include <osg/Matrix>
#include <osg/Vec2d>
#include <osg/Vec3d>
#include <glm/gtc/type_ptr.hpp>

namespace czmosg
{

    inline void setMat4d(osg::Matrixd& matd, const glm::dmat4x4& glmmat)
    {
        std::memcpy(matd.ptr(), glm::value_ptr(glmmat), sizeof(double) * 16);
    }

    inline osg::Matrixd glm2osg(const glm::dmat4x4& glmmat)
    {
        osg::Matrixd result;
        setMat4d(result, glmmat);
        return result;
    }

    inline glm::dmat4x4 osg2glm(const osg::Matrixd& osgmat)
    {
        glm::dmat4x4 result{};
        std::memcpy(glm::value_ptr(result), osgmat.ptr(), sizeof(double) * 16);
        return result;
    }

    inline osg::Vec2d glm2osg(const glm::dvec2& vec2)
    {
        return osg::Vec2d(vec2.x, vec2.y);
    }

    inline osg::Vec3d glm2osg(const glm::dvec3& vec3)
    {
        return osg::Vec3d(vec3.x, vec3.y, vec3.z);
    }

    inline glm::dvec2 osg2glm(const osg::Vec2d& vec2)
    {
        return glm::dvec2(vec2.x(), vec2.y());
    }

    inline glm::dvec3 osg2glm(const osg::Vec3d& vec3)
    {
        return glm::dvec3(vec3.x(), vec3.y(), vec3.z());
    }

    inline bool isIdentity(const glm::dmat4x4& mat)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                if (c == r)
                {
                    if (mat[c][r] != 1.0)
                        return false;
                }
                else
                {
                    if (mat[c][r] != 0.0)
                        return false;
                }
            }
        }
        return true;
    }

    static thread_local bool _isMainThread = false;

    bool isMainThread()
    {
        return _isMainThread;
    }

    void setMainThread()
    {
        _isMainThread = true;
    }
}