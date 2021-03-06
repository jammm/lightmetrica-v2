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

#include <pch.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/property.h>
#include <lightmetrica/spectrum.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/bsdfutils.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/texture.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/detail/serial.h>

LM_NAMESPACE_BEGIN

class BSDF_Flesnel final : public BSDF
{
public:

    LM_IMPL_CLASS(BSDF_Flesnel, BSDF);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        R_ = SPD::FromRGB(prop->ChildAs<Vec3>("R", Vec3()));
        eta1_ = prop->ChildAs<Float>("eta1", 1_f);
        eta2_ = prop->ChildAs<Float>("eta2", 2_f);
        return true;
    };

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::S;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        const auto localWi = geom.ToLocal * wi;

        // IORs
        Float etaI = eta1_;
        Float etaT = eta2_;
        if (Math::LocalCos(localWi) < 0_f)
        {
            std::swap(etaI, etaT);
        }

        // Fresnel term
        const Float Fr = EvaluateFresnelTerm(localWi, etaI, etaT);
        if (uComp <= Fr)
        {
            // Reflection
            const auto localWo = BSDFUtils::LocalReflect(localWi);
            wo = geom.ToWorld * localWo;
        }
        else
        {
            // Refraction
            const Float wiDotN = Math::LocalCos(localWi);
            const Float eta = etaI / etaT;
            const Float cosThetaTSq = 1_f - eta * eta * (1_f - wiDotN * wiDotN);
            assert(cosThetaTSq >= 0);
            const Float cosThetaT = Math::Sqrt(cosThetaTSq) * (wiDotN > 0_f ? -1_f : 1_f);
            const auto localWo = BSDFUtils::LocalRefract(localWi, eta, cosThetaT);
            wo = geom.ToWorld * localWo;
        }
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        if (evalDelta)
        {
            return PDFVal(PDFMeasure::ProjectedSolidAngle, 0_f);
        }

        // Local directions
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;

        // IORs
        Float etaI = eta1_;
        Float etaT = eta2_;
        if (Math::LocalCos(localWi) < 0_f)
        {
            std::swap(etaI, etaT);
        }

        // Fresnel term
        const Float Fr = EvaluateFresnelTerm(localWi, etaI, etaT);
        if (Math::LocalCos(localWi) * Math::LocalCos(localWo) >= 0_f)
        {
            // Reflection
            return PDFVal(PDFMeasure::ProjectedSolidAngle, Fr);
        }
        else
        {
            // Refraction
            return PDFVal(PDFMeasure::ProjectedSolidAngle, 1_f - Fr);
        }

        LM_UNREACHABLE();
        return PDFVal(PDFMeasure::ProjectedSolidAngle, 0_f);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        if (evalDelta)
        {
            return SPD();
        }

        // Local directions
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;

        // IORs
        Float etaI = eta1_;
        Float etaT = eta2_;
        if (Math::LocalCos(localWi) < 0_f)
        {
            std::swap(etaI, etaT);
        }

        // Fresnel term
        const Float Fr = EvaluateFresnelTerm(localWi, etaI, etaT);
        if (Math::LocalCos(localWi) * Math::LocalCos(localWo) >= 0_f)
        {
            // Reflection
            return R_ * Fr * BSDFUtils::ShadingNormalCorrection(geom, wi, wo, transDir);
        }
        else
        {
            // Refraction
            const Float eta = etaI / etaT;
            const auto refrCorrection = transDir == TransportDirection::EL ? eta : 1_f;
            return R_ * (1_f - Fr) * BSDFUtils::ShadingNormalCorrection(geom, wi, wo, transDir) * refrCorrection * refrCorrection;
        }

        LM_UNREACHABLE();
        return SPD();
    };

    LM_IMPL_F(IsDeltaDirection) = [this](int type) -> bool
    {
        return true;
    };

    LM_IMPL_F(IsDeltaPosition) = [this](int type) -> bool
    {
        return false;
    };

    LM_IMPL_F(Serialize) = [this](std::ostream& stream) -> bool
    {
        {
            cereal::PortableBinaryOutputArchive oa(stream);
            oa(R_, eta1_, eta2_);
        }
        return true;
    };

    LM_IMPL_F(Deserialize) = [this](std::istream& stream, const std::unordered_map<std::string, void*>& userdata) -> bool
    {
        {
            cereal::PortableBinaryInputArchive ia(stream);
            ia(R_, eta1_, eta2_);
        }
        return true;
    };

    LM_IMPL_F(FlesnelTerm) = [this](const SurfaceGeometry& geom, const Vec3& wi) -> Float
    {
        // Local directions
        const auto localWi = geom.ToLocal * wi;

        // IORs
        Float etaI = eta1_;
        Float etaT = eta2_;
        if (Math::LocalCos(localWi) < 0_f)
        {
            std::swap(etaI, etaT);
        }

        // Fresnel term
        return EvaluateFresnelTerm(localWi, etaI, etaT);
    };

    LM_IMPL_F(Eta) = [this](const SurfaceGeometry& geom, const Vec3& wi) -> Float
    {
        Float etaI = eta1_;
        Float etaT = eta2_;
        const auto localWi = geom.ToLocal * wi;
        if (Math::LocalCos(localWi) < 0_f)
        {
            std::swap(etaI, etaT);
        }
        return etaI / etaT;
    };

private:

    auto EvaluateFresnelTerm(const Vec3& localWi, Float etaI, Float etaT) const -> Float
    {
        const Float wiDotN = Math::LocalCos(localWi);
        const Float eta = etaI / etaT;
        const Float cosThetaTSq = 1_f - eta * eta * (1_f - wiDotN * wiDotN);
        if (cosThetaTSq <= 0_f)
        {
            return 1_f;
        }

        const Float absCosThetaI = std::abs(wiDotN);
        const Float absCosThetaT = std::sqrt(cosThetaTSq);
        const Float rhoS = (etaI * absCosThetaI - etaT * absCosThetaT) / (etaI * absCosThetaI + etaT * absCosThetaT);
        const Float rhoT = (etaI * absCosThetaT - etaT * absCosThetaI) / (etaI * absCosThetaT + etaT * absCosThetaI);

        return (rhoS * rhoS + rhoT * rhoT) * .5_f;
    }

public:

    SPD R_;
    Float eta1_;
    Float eta2_;

};

LM_COMPONENT_REGISTER_IMPL(BSDF_Flesnel, "bsdf::flesnel");

LM_NAMESPACE_END
