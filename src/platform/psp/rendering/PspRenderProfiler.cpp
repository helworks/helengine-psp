#include "platform/psp/rendering/PspRenderProfiler.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <pspkernel.h>

#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
#if defined(HELENGINE_PSP_ENABLE_RENDER_PROFILER) && HELENGINE_PSP_ENABLE_RENDER_PROFILER
    namespace {
        constexpr int32_t WarmupFrameCount = 90;
        constexpr int32_t DetailedCaptureFrameCount = 5;
        constexpr int32_t SummaryIntervalFrames = 30;

        struct MetricStat {
            int32_t Count = 0;
            std::uint64_t TotalMicroseconds = 0;
            std::uint64_t MaxMicroseconds = 0;
        };

        struct FrameProfileState {
            int32_t FrameIndex = 0;
            bool FrameActive = false;
            bool IsDetailedFrame = false;

            MetricStat TwoDTextureBind;
            MetricStat TwoDTextureFlush;
            MetricStat ThreeDTextureBind;
            MetricStat ThreeDTextureFlush;
            MetricStat TwoDTexturedQuad;
            MetricStat TwoDQuadDrawArray;
            MetricStat TwoDTexturedTriangles;
            MetricStat TwoDTriangleDrawArray;
            MetricStat TwoDText;
            MetricStat TwoDCamera;
            MetricStat ThreeDSceneLightResolve;
            MetricStat ThreeDCameraSetup;
            MetricStat ThreeDQueueVisit;
            MetricStat ThreeDTimestampCalibration;
            MetricStat ThreeDMaterialResolve;
            MetricStat ThreeDWorldMatrixBuild;
            MetricStat ThreeDModelMatrixLoad;
            MetricStat ThreeDFixedFunctionMaterialSetup;
            MetricStat ThreeDFixedFunctionDraw;
            MetricStat ThreeDVisit;
            MetricStat ThreeDCamera;
            MetricStat ThreeDCameraUi;

            std::uint64_t TwoDTextureBindBytes = 0;
            std::uint64_t ThreeDTextureBindBytes = 0;
            int32_t TwoDRepeatedTextureBinds = 0;
            int32_t ThreeDRepeatedTextureBinds = 0;
            int32_t TwoDTextGlyphCount = 0;
            int32_t TwoDTextContentLength = 0;
            int32_t TwoDDrawableCount = 0;
            int32_t ThreeDDrawableCount = 0;
            std::uintptr_t Last2DTexturePointer = 0;
            std::uintptr_t Last3DTexturePointer = 0;
            std::vector<std::string> DetailedLines;
        };

        FrameProfileState CurrentFrame;

        void ResetMetric(MetricStat& metric) {
            metric.Count = 0;
            metric.TotalMicroseconds = 0;
            metric.MaxMicroseconds = 0;
        }

        void ResetFrameState() {
            ResetMetric(CurrentFrame.TwoDTextureBind);
            ResetMetric(CurrentFrame.TwoDTextureFlush);
            ResetMetric(CurrentFrame.ThreeDTextureBind);
            ResetMetric(CurrentFrame.ThreeDTextureFlush);
            ResetMetric(CurrentFrame.TwoDTexturedQuad);
            ResetMetric(CurrentFrame.TwoDQuadDrawArray);
            ResetMetric(CurrentFrame.TwoDTexturedTriangles);
            ResetMetric(CurrentFrame.TwoDTriangleDrawArray);
            ResetMetric(CurrentFrame.TwoDText);
            ResetMetric(CurrentFrame.TwoDCamera);
            ResetMetric(CurrentFrame.ThreeDSceneLightResolve);
            ResetMetric(CurrentFrame.ThreeDCameraSetup);
            ResetMetric(CurrentFrame.ThreeDQueueVisit);
            ResetMetric(CurrentFrame.ThreeDTimestampCalibration);
            ResetMetric(CurrentFrame.ThreeDMaterialResolve);
            ResetMetric(CurrentFrame.ThreeDWorldMatrixBuild);
            ResetMetric(CurrentFrame.ThreeDModelMatrixLoad);
            ResetMetric(CurrentFrame.ThreeDFixedFunctionMaterialSetup);
            ResetMetric(CurrentFrame.ThreeDFixedFunctionDraw);
            ResetMetric(CurrentFrame.ThreeDVisit);
            ResetMetric(CurrentFrame.ThreeDCamera);
            ResetMetric(CurrentFrame.ThreeDCameraUi);
            CurrentFrame.TwoDTextureBindBytes = 0;
            CurrentFrame.ThreeDTextureBindBytes = 0;
            CurrentFrame.TwoDRepeatedTextureBinds = 0;
            CurrentFrame.ThreeDRepeatedTextureBinds = 0;
            CurrentFrame.TwoDTextGlyphCount = 0;
            CurrentFrame.TwoDTextContentLength = 0;
            CurrentFrame.TwoDDrawableCount = 0;
            CurrentFrame.ThreeDDrawableCount = 0;
            CurrentFrame.Last2DTexturePointer = 0;
            CurrentFrame.Last3DTexturePointer = 0;
            CurrentFrame.DetailedLines.clear();
        }

        void RecordMetric(MetricStat& metric, std::uint64_t microseconds) {
            metric.Count++;
            metric.TotalMicroseconds += microseconds;
            metric.MaxMicroseconds = std::max(metric.MaxMicroseconds, microseconds);
        }

        std::uint64_t AverageMicroseconds(const MetricStat& metric) {
            if (metric.Count <= 0) {
                return 0;
            }

            return metric.TotalMicroseconds / static_cast<std::uint64_t>(metric.Count);
        }

        std::string DescribeTexture(PspRuntimeTexture* texture) {
            if (texture == nullptr) {
                return "texture=null";
            }

            return std::string("textureId=") + texture->get_Id()
                + " ptr=" + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
                + " width=" + std::to_string(texture->get_Width())
                + " height=" + std::to_string(texture->get_Height())
                + " pixels=" + std::to_string(texture->GetPixelCount());
        }

        void AppendDetailedLine(const std::string& line) {
            if (!CurrentFrame.IsDetailedFrame) {
                return;
            }

            CurrentFrame.DetailedLines.push_back(line);
        }

        std::string DescribeMetric(const char* name, const MetricStat& metric) {
            return std::string(name)
                + "Count=" + std::to_string(metric.Count)
                + "TotalUs=" + std::to_string(metric.TotalMicroseconds)
                + "AvgUs=" + std::to_string(AverageMicroseconds(metric))
                + "MaxUs=" + std::to_string(metric.MaxMicroseconds);
        }

        bool ShouldEmitSummary() {
            if (CurrentFrame.IsDetailedFrame) {
                return true;
            }

            return CurrentFrame.FrameIndex > WarmupFrameCount
                && ((CurrentFrame.FrameIndex - WarmupFrameCount) % SummaryIntervalFrames) == 0;
        }
    }

    /// Returns the current PSP system timestamp in microseconds.
    std::uint64_t PspRenderProfiler::GetTimestampMicroseconds() {
        return static_cast<std::uint64_t>(sceKernelGetSystemTimeWide());
    }

    /// Starts a new renderer frame capture.
    void PspRenderProfiler::BeginFrame() {
        CurrentFrame.FrameIndex++;
        CurrentFrame.FrameActive = true;
        CurrentFrame.IsDetailedFrame = CurrentFrame.FrameIndex > WarmupFrameCount
            && CurrentFrame.FrameIndex <= (WarmupFrameCount + DetailedCaptureFrameCount);
        ResetFrameState();
    }

    /// Completes the current renderer frame capture and emits any configured trace output.
    void PspRenderProfiler::EndFrame() {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        if (ShouldEmitSummary()) {
            const char* detailMode = CurrentFrame.IsDetailedFrame ? "detail" : "summary";
            PspBootTrace::WriteLine(
                std::string("PspPerfFrame frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " mode=" + detailMode
                + " twoDDrawables=" + std::to_string(CurrentFrame.TwoDDrawableCount)
                + " threeDDrawables=" + std::to_string(CurrentFrame.ThreeDDrawableCount)
                + " glyphs=" + std::to_string(CurrentFrame.TwoDTextGlyphCount)
                + " textChars=" + std::to_string(CurrentFrame.TwoDTextContentLength)
                + " twoDBindBytes=" + std::to_string(CurrentFrame.TwoDTextureBindBytes)
                + " threeDBindBytes=" + std::to_string(CurrentFrame.ThreeDTextureBindBytes)
                + " twoDRepeatBinds=" + std::to_string(CurrentFrame.TwoDRepeatedTextureBinds)
                + " threeDRepeatBinds=" + std::to_string(CurrentFrame.ThreeDRepeatedTextureBinds));
            PspBootTrace::WriteLine(
                std::string("PspPerfFrame2D frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " " + DescribeMetric("camera", CurrentFrame.TwoDCamera)
                + " " + DescribeMetric("text", CurrentFrame.TwoDText)
                + " " + DescribeMetric("quad", CurrentFrame.TwoDTexturedQuad)
                + " " + DescribeMetric("quadDraw", CurrentFrame.TwoDQuadDrawArray)
                + " " + DescribeMetric("tri", CurrentFrame.TwoDTexturedTriangles)
                + " " + DescribeMetric("triDraw", CurrentFrame.TwoDTriangleDrawArray)
                + " " + DescribeMetric("bind", CurrentFrame.TwoDTextureBind)
                + " " + DescribeMetric("flush", CurrentFrame.TwoDTextureFlush));
            PspBootTrace::WriteLine(
                std::string("PspPerfFrame3D frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " " + DescribeMetric("sceneLight", CurrentFrame.ThreeDSceneLightResolve)
                + " " + DescribeMetric("setup", CurrentFrame.ThreeDCameraSetup)
                + " " + DescribeMetric("queue", CurrentFrame.ThreeDQueueVisit)
                + " " + DescribeMetric("material", CurrentFrame.ThreeDMaterialResolve)
                + " " + DescribeMetric("world", CurrentFrame.ThreeDWorldMatrixBuild)
                + " " + DescribeMetric("modelLoad", CurrentFrame.ThreeDModelMatrixLoad)
                + " " + DescribeMetric("ffMaterial", CurrentFrame.ThreeDFixedFunctionMaterialSetup)
                + " " + DescribeMetric("ffDraw", CurrentFrame.ThreeDFixedFunctionDraw)
                + " " + DescribeMetric("visit", CurrentFrame.ThreeDVisit)
                + " " + DescribeMetric("camera", CurrentFrame.ThreeDCamera)
                + " " + DescribeMetric("ui", CurrentFrame.ThreeDCameraUi)
                + " " + DescribeMetric("bind", CurrentFrame.ThreeDTextureBind)
                + " " + DescribeMetric("flush", CurrentFrame.ThreeDTextureFlush));
            for (const std::string& line : CurrentFrame.DetailedLines) {
                PspBootTrace::WriteLine(line);
            }
        }

        CurrentFrame.FrameActive = false;
    }

    /// Records one 2D texture bind, including total bind time and the texture writeback portion.
    void PspRenderProfiler::Record2DTextureBind(PspRuntimeTexture* texture, std::size_t byteCount, std::uint64_t totalMicroseconds, std::uint64_t flushMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.TwoDTextureBind, totalMicroseconds);
        RecordMetric(CurrentFrame.TwoDTextureFlush, flushMicroseconds);
        CurrentFrame.TwoDTextureBindBytes += byteCount;
        const std::uintptr_t texturePointer = reinterpret_cast<std::uintptr_t>(texture);
        if (texturePointer != 0 && texturePointer == CurrentFrame.Last2DTexturePointer) {
            CurrentFrame.TwoDRepeatedTextureBinds++;
        }

        CurrentFrame.Last2DTexturePointer = texturePointer;
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=2D op=BindTexture totalUs=" + std::to_string(totalMicroseconds)
                + " flushUs=" + std::to_string(flushMicroseconds)
                + " bytes=" + std::to_string(byteCount)
                + " " + DescribeTexture(texture));
        }
    }

    /// Records one 3D texture bind, including total bind time and the texture writeback portion.
    void PspRenderProfiler::Record3DTextureBind(PspRuntimeTexture* texture, std::size_t byteCount, std::uint64_t totalMicroseconds, std::uint64_t flushMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDTextureBind, totalMicroseconds);
        RecordMetric(CurrentFrame.ThreeDTextureFlush, flushMicroseconds);
        CurrentFrame.ThreeDTextureBindBytes += byteCount;
        const std::uintptr_t texturePointer = reinterpret_cast<std::uintptr_t>(texture);
        if (texturePointer != 0 && texturePointer == CurrentFrame.Last3DTexturePointer) {
            CurrentFrame.ThreeDRepeatedTextureBinds++;
        }

        CurrentFrame.Last3DTexturePointer = texturePointer;
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=3D op=BindTexture totalUs=" + std::to_string(totalMicroseconds)
                + " flushUs=" + std::to_string(flushMicroseconds)
                + " bytes=" + std::to_string(byteCount)
                + " " + DescribeTexture(texture));
        }
    }

    /// Records one 2D textured-quad draw.
    void PspRenderProfiler::Record2DTexturedQuad(PspRuntimeTexture* texture, const int2& size, std::uint64_t totalMicroseconds, std::uint64_t drawMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.TwoDTexturedQuad, totalMicroseconds);
        RecordMetric(CurrentFrame.TwoDQuadDrawArray, drawMicroseconds);
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=2D op=DrawTexturedQuad totalUs=" + std::to_string(totalMicroseconds)
                + " drawUs=" + std::to_string(drawMicroseconds)
                + " size=" + std::to_string(size.X) + "x" + std::to_string(size.Y)
                + " " + DescribeTexture(texture));
        }
    }

    /// Records one 2D textured-triangle draw.
    void PspRenderProfiler::Record2DTexturedTriangles(PspRuntimeTexture* texture, std::size_t vertexCount, std::uint64_t totalMicroseconds, std::uint64_t drawMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.TwoDTexturedTriangles, totalMicroseconds);
        RecordMetric(CurrentFrame.TwoDTriangleDrawArray, drawMicroseconds);
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=2D op=DrawTexturedTriangles totalUs=" + std::to_string(totalMicroseconds)
                + " drawUs=" + std::to_string(drawMicroseconds)
                + " vertices=" + std::to_string(vertexCount)
                + " " + DescribeTexture(texture));
        }
    }

    /// Records one 2D text draw and the amount of visible glyph work it submitted.
    void PspRenderProfiler::Record2DText(int32_t glyphCount, int32_t contentLength, std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.TwoDText, totalMicroseconds);
        CurrentFrame.TwoDTextGlyphCount += glyphCount;
        CurrentFrame.TwoDTextContentLength += contentLength;
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=2D op=DrawText totalUs=" + std::to_string(totalMicroseconds)
                + " glyphs=" + std::to_string(glyphCount)
                + " chars=" + std::to_string(contentLength));
        }
    }

    /// Records one complete 2D camera pass.
    void PspRenderProfiler::Record2DCamera(int32_t drawableCount, std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.TwoDCamera, totalMicroseconds);
        CurrentFrame.TwoDDrawableCount += drawableCount;
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=2D op=RenderCamera totalUs=" + std::to_string(totalMicroseconds)
                + " drawables=" + std::to_string(drawableCount));
        }
    }

    /// Records one scene-light resolve pass inside the 3D camera.
    void PspRenderProfiler::Record3DSceneLightResolve(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDSceneLightResolve, totalMicroseconds);
    }

    /// Records one pre-queue 3D camera setup phase before scene-light resolve and drawable traversal.
    void PspRenderProfiler::Record3DCameraSetup(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDCameraSetup, totalMicroseconds);
    }

    /// Records one ordered 3D render-queue traversal call.
    void PspRenderProfiler::Record3DQueueVisit(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDQueueVisit, totalMicroseconds);
    }

    /// Records one runtime-material root resolve inside one 3D drawable visit.
    void PspRenderProfiler::Record3DMaterialResolve(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDMaterialResolve, totalMicroseconds);
    }

    /// Records one world-matrix build inside one 3D drawable visit.
    void PspRenderProfiler::Record3DWorldMatrixBuild(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDWorldMatrixBuild, totalMicroseconds);
    }

    /// Records one GU model-matrix load inside one 3D drawable visit.
    void PspRenderProfiler::Record3DModelMatrixLoad(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDModelMatrixLoad, totalMicroseconds);
    }

    /// Records one fixed-function material-state setup inside one 3D drawable visit.
    void PspRenderProfiler::Record3DFixedFunctionMaterialSetup(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDFixedFunctionMaterialSetup, totalMicroseconds);
    }

    /// Records one fixed-function GU draw submission inside one 3D drawable visit.
    void PspRenderProfiler::Record3DFixedFunctionDraw(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDFixedFunctionDraw, totalMicroseconds);
    }

    /// Records one complete 3D drawable visit.
    void PspRenderProfiler::Record3DVisit(std::uint64_t totalMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDVisit, totalMicroseconds);
    }

    /// Records one complete 3D camera pass and the time spent in its nested 2D UI phase.
    void PspRenderProfiler::Record3DCamera(int32_t drawableCount, std::uint64_t totalMicroseconds, std::uint64_t uiMicroseconds) {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        RecordMetric(CurrentFrame.ThreeDCamera, totalMicroseconds);
        RecordMetric(CurrentFrame.ThreeDCameraUi, uiMicroseconds);
        CurrentFrame.ThreeDDrawableCount += drawableCount;
        if (CurrentFrame.IsDetailedFrame) {
            AppendDetailedLine(
                std::string("PspPerfCall frame=") + std::to_string(CurrentFrame.FrameIndex)
                + " category=3D op=RenderCamera totalUs=" + std::to_string(totalMicroseconds)
                + " uiUs=" + std::to_string(uiMicroseconds)
                + " drawables=" + std::to_string(drawableCount));
        }
    }
#endif
}
