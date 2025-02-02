// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2020-2021 Intel Corporation

#ifndef OPENCV_GAPI_INFER_ONNX_HPP
#define OPENCV_GAPI_INFER_ONNX_HPP

#include <unordered_map>
#include <string>
#include <array>
#include <tuple> // tuple, tuple_size

#include <opencv2/gapi/opencv_includes.hpp>
#include <opencv2/gapi/util/any.hpp>

#include <opencv2/core/cvdef.h>     // GAPI_EXPORTS
#include <opencv2/gapi/gkernel.hpp> // GKernelPackage
#include <opencv2/gapi/infer.hpp>   // Generic

namespace cv {
namespace gapi {

/**
 * @brief This namespace contains G-API ONNX Runtime backend functions, structures, and symbols.
 */
namespace onnx {

/**
 * @brief This namespace contains Execution Providers structures for G-API ONNX Runtime backend.
 */
namespace ep {

/**
 * @brief This structure provides functions
 * that fill inference options for ONNX OpenVINO Execution Provider.
 * Please follow https://onnxruntime.ai/docs/execution-providers/OpenVINO-ExecutionProvider.html#summary-of-options
 */
struct GAPI_EXPORTS_W_SIMPLE OpenVINO {
    // NB: Used from python.
    /// @private -- Exclude this constructor from OpenCV documentation
    GAPI_WRAP
    OpenVINO() = default;

    /** @brief Class constructor.

    Constructs OpenVINO parameters based on device information.

    @param device Target device to use.
    */
    GAPI_WRAP
    OpenVINO(const std::string &device)
        : device_id(device) {
    }

    /** @brief Specifies OpenVINO Execution Provider device type.

    This function is used to override the accelerator hardware type
    and precision at runtime. If this option is not explicitly configured, default
    hardware and precision specified during onnxruntime build time is used.

    @param type Device type ("CPU_FP32", "GPU_FP16", etc)
    @return reference to this parameter structure.
    */
    GAPI_WRAP
    OpenVINO& cfgDeviceType(const std::string &type) {
        device_type = cv::util::make_optional(type);
        return *this;
    }

    /** @brief Specifies OpenVINO Execution Provider cache dir.

    This function is used to explicitly specify the path to save and load
    the blobs enabling model caching feature.

    @param dir Path to the directory what will be used as cache.
    @return reference to this parameter structure.
    */
    GAPI_WRAP
    OpenVINO& cfgCacheDir(const std::string &dir) {
        cache_dir = dir;
        return *this;
    }

    /** @brief Specifies OpenVINO Execution Provider number of threads.

    This function is used to override the accelerator default value
    of number of threads with this value at runtime. If this option
    is not explicitly set, default value of 8 is used during build time.

    @param nthreads Number of threads.
    @return reference to this parameter structure.
    */
    GAPI_WRAP
    OpenVINO& cfgNumThreads(size_t nthreads) {
        num_of_threads = cv::util::make_optional(nthreads);
        return *this;
    }

    /** @brief Enables OpenVINO Execution Provider opencl throttling.

    This function is used to enable OpenCL queue throttling for GPU devices
    (reduces CPU utilization when using GPU).

    @return reference to this parameter structure.
    */
    GAPI_WRAP
    OpenVINO& cfgEnableOpenCLThrottling() {
        enable_opencl_throttling = true;
        return *this;
    }

    /** @brief Enables OpenVINO Execution Provider dynamic shapes.

    This function is used to enable OpenCL queue throttling for GPU devices
    (reduces CPU utilization when using GPU).
    This function is used to enable work with dynamic shaped models
    whose shape will be set dynamically based on the infer input
    image/data shape at run time in CPU.

    @return reference to this parameter structure.
    */
    GAPI_WRAP
    OpenVINO& cfgEnableDynamicShapes() {
        enable_dynamic_shapes = true;
        return *this;
    }

    std::string device_id;
    std::string cache_dir;
    cv::optional<std::string> device_type;
    cv::optional<size_t> num_of_threads;
    bool enable_opencl_throttling = false;
    bool enable_dynamic_shapes = false;
};

using EP = cv::util::variant<cv::util::monostate, OpenVINO>;

} // namespace ep

GAPI_EXPORTS cv::gapi::GBackend backend();

enum class TraitAs: int {
    TENSOR, //!< G-API traits an associated cv::Mat as a raw tensor
            // and passes dimensions as-is
    IMAGE   //!< G-API traits an associated cv::Mat as an image so
            // creates an "image" blob (NCHW/NHWC, etc)
};

using PostProc = std::function<void(const std::unordered_map<std::string, cv::Mat> &,
                                          std::unordered_map<std::string, cv::Mat> &)>;

namespace detail {
/**
* @brief This structure contains description of inference parameters
* which is specific to ONNX models.
*/
struct ParamDesc {
    std::string model_path; //!< Path to model.

    // NB: nun_* may differ from topology's real input/output port numbers
    // (e.g. topology's partial execution)
    std::size_t num_in;  //!< How many inputs are defined in the operation
    std::size_t num_out; //!< How many outputs are defined in the operation

    // NB: Here order follows the `Net` API
    std::vector<std::string> input_names; //!< Names of input network layers.
    std::vector<std::string> output_names; //!< Names of output network layers.

    using ConstInput = std::pair<cv::Mat, TraitAs>;
    std::unordered_map<std::string, ConstInput> const_inputs; //!< Map with pair of name of network layer and ConstInput which will be associated with this.

    std::vector<cv::Scalar> mean; //!< Mean values for preprocessing.
    std::vector<cv::Scalar> stdev; //!< Standard deviation values for preprocessing.

    std::vector<cv::GMatDesc> out_metas; //!< Out meta information about your output (type, dimension).
    PostProc custom_post_proc; //!< Post processing function.

    std::vector<bool> normalize; //!< Vector of bool values that enabled or disabled normalize of input data.

    std::vector<std::string> names_to_remap; //!< Names of output layers that will be processed in PostProc function.

    bool is_generic;

    // TODO: Needs to modify the rest of ParamDesc accordingly to support
    // both generic and non-generic options without duplication
    // (as it was done for the OV IE backend)
    // These values are pushed into the respective vector<> fields above
    // when the generic infer parameters are unpacked (see GONNXBackendImpl::unpackKernel)
    std::unordered_map<std::string, std::pair<cv::Scalar, cv::Scalar> > generic_mstd;
    std::unordered_map<std::string, bool> generic_norm;

    cv::gapi::onnx::ep::EP execution_provider;
    bool disable_mem_pattern;
};
} // namespace detail

template<typename Net>
struct PortCfg {
    using In = std::array
        < std::string
        , std::tuple_size<typename Net::InArgs>::value >;
    using Out = std::array
        < std::string
        , std::tuple_size<typename Net::OutArgs>::value >;
    using NormCoefs = std::array
        < cv::Scalar
        , std::tuple_size<typename Net::InArgs>::value >;
    using Normalize = std::array
        < bool
        , std::tuple_size<typename Net::InArgs>::value >;
};

/**
 * Contains description of inference parameters and kit of functions that
 * fill this parameters.
 */
template<typename Net> class Params {
public:
    /** @brief Class constructor.

    Constructs Params based on model information and sets default values for other
    inference description parameters.

    @param model Path to model (.onnx file).
    */
    Params(const std::string &model) {
        desc.model_path = model;
        desc.num_in  = std::tuple_size<typename Net::InArgs>::value;
        desc.num_out = std::tuple_size<typename Net::OutArgs>::value;
        desc.is_generic = false;
        desc.disable_mem_pattern = false;
    };

    /** @brief Specifies sequence of network input layers names for inference.

    The function is used to associate data of graph inputs with input layers of
    network topology. Number of names has to match the number of network inputs. If a network
    has only one input layer, there is no need to call it as the layer is
    associated with input automatically but this doesn't prevent you from
    doing it yourself. Count of names has to match to number of network inputs.

    @param layer_names std::array<std::string, N> where N is the number of inputs
    as defined in the @ref G_API_NET. Contains names of input layers.
    @return the reference on modified object.
    */
    Params<Net>& cfgInputLayers(const typename PortCfg<Net>::In &layer_names) {
        desc.input_names.assign(layer_names.begin(), layer_names.end());
        return *this;
    }

    /** @brief Specifies sequence of output layers names for inference.

     The function is used to associate data of graph outputs with output layers of
    network topology. If a network has only one output layer, there is no need to call it
    as the layer is associated with output automatically but this doesn't prevent
    you from doing it yourself. Count of names has to match to number of network
    outputs or you can set your own output but for this case you have to
    additionally use @ref cfgPostProc function.

    @param layer_names std::array<std::string, N> where N is the number of outputs
    as defined in the @ref G_API_NET. Contains names of output layers.
    @return the reference on modified object.
    */
    Params<Net>& cfgOutputLayers(const typename PortCfg<Net>::Out &layer_names) {
        desc.output_names.assign(layer_names.begin(), layer_names.end());
        return *this;
    }

    /** @brief Sets a constant input.

    The function is used to set constant input. This input has to be
    a prepared tensor since preprocessing is disabled for this case. You should
    provide name of network layer which will receive provided data.

    @param layer_name Name of network layer.
    @param data cv::Mat that contains data which will be associated with network layer.
    @param hint Type of input (TENSOR).
    @return the reference on modified object.
    */
    Params<Net>& constInput(const std::string &layer_name,
                            const cv::Mat &data,
                            TraitAs hint = TraitAs::TENSOR) {
        desc.const_inputs[layer_name] = {data, hint};
        return *this;
    }

    /** @brief Specifies mean value and standard deviation for preprocessing.

    The function is used to set mean value and standard deviation for preprocessing
    of input data.

    @param m std::array<cv::Scalar, N> where N is the number of inputs
    as defined in the @ref G_API_NET. Contains mean values.
    @param s std::array<cv::Scalar, N> where N is the number of inputs
    as defined in the @ref G_API_NET. Contains standard deviation values.
    @return the reference on modified object.
    */
    Params<Net>& cfgMeanStd(const typename PortCfg<Net>::NormCoefs &m,
                            const typename PortCfg<Net>::NormCoefs &s) {
        desc.mean.assign(m.begin(), m.end());
        desc.stdev.assign(s.begin(), s.end());
        return *this;
    }

    /** @brief Configures graph output and provides the post processing function from user.

    The function is used when you work with networks with dynamic outputs.
    Since we can't know dimensions of inference result needs provide them for
    construction of graph output. This dimensions can differ from inference result.
    So you have to provide @ref PostProc function that gets information from inference
    result and fill output which is constructed by dimensions from out_metas.

    @param out_metas Out meta information about your output (type, dimension).
    @param remap_function Post processing function, which has two parameters. First is onnx
    result, second is graph output. Both parameters is std::map that contain pair of
    layer's name and cv::Mat.
    @return the reference on modified object.
    */
    Params<Net>& cfgPostProc(const std::vector<cv::GMatDesc> &out_metas,
                             const PostProc &remap_function) {
        desc.out_metas        = out_metas;
        desc.custom_post_proc = remap_function;
        return *this;
    }

    /** @overload
    Function with a rvalue parameters.

    @param out_metas rvalue out meta information about your output (type, dimension).
    @param remap_function rvalue post processing function, which has two parameters. First is onnx
    result, second is graph output. Both parameters is std::map that contain pair of
    layer's name and cv::Mat.
    @return the reference on modified object.
    */
    Params<Net>& cfgPostProc(std::vector<cv::GMatDesc> &&out_metas,
                             PostProc &&remap_function) {
        desc.out_metas        = std::move(out_metas);
        desc.custom_post_proc = std::move(remap_function);
        return *this;
    }

    /** @overload
    The function has additional parameter names_to_remap. This parameter provides
    information about output layers which will be used for inference and post
    processing function.

    @param out_metas Out meta information.
    @param remap_function Post processing function.
    @param names_to_remap Names of output layers. network's inference will
    be done on these layers. Inference's result will be processed in post processing
    function using these names.
    @return the reference on modified object.
    */
    Params<Net>& cfgPostProc(const std::vector<cv::GMatDesc> &out_metas,
                             const PostProc &remap_function,
                             const std::vector<std::string> &names_to_remap) {
        desc.out_metas        = out_metas;
        desc.custom_post_proc = remap_function;
        desc.names_to_remap   = names_to_remap;
        return *this;
    }

    /** @overload
    Function with a rvalue parameters and additional parameter names_to_remap.

    @param out_metas rvalue out meta information.
    @param remap_function rvalue post processing function.
    @param names_to_remap rvalue names of output layers. network's inference will
    be done on these layers. Inference's result will be processed in post processing
    function using these names.
    @return the reference on modified object.
    */
    Params<Net>& cfgPostProc(std::vector<cv::GMatDesc> &&out_metas,
                             PostProc &&remap_function,
                             std::vector<std::string> &&names_to_remap) {
        desc.out_metas        = std::move(out_metas);
        desc.custom_post_proc = std::move(remap_function);
        desc.names_to_remap   = std::move(names_to_remap);
        return *this;
    }

    /** @brief Specifies normalize parameter for preprocessing.

    The function is used to set normalize parameter for preprocessing of input data.

    @param normalizations std::array<cv::Scalar, N> where N is the number of inputs
    as defined in the @ref G_API_NET. Сontains bool values that enabled or disabled
    normalize of input data.
    @return the reference on modified object.
    */
    Params<Net>& cfgNormalize(const typename PortCfg<Net>::Normalize &normalizations) {
        desc.normalize.assign(normalizations.begin(), normalizations.end());
        return *this;
    }

    /** @brief Specifies execution provider for runtime.

    The function is used to set ONNX Runtime OpenVINO Execution Provider options.

    @param ovep OpenVINO Execution Provider options.
    @see cv::gapi::onnx::ep::OpenVINO.

    @return the reference on modified object.
    */
    Params<Net>& cfgExecutionProvider(ep::OpenVINO&& ovep) {
        desc.execution_provider = std::move(ovep);
        return *this;
    }

    /** @brief Disables the memory pattern optimization.

    @return the reference on modified object.
    */
    Params<Net>& cfgDisableMemPattern() {
        desc.disable_mem_pattern = true;
        return *this;
    }

    // BEGIN(G-API's network parametrization API)
    GBackend      backend() const { return cv::gapi::onnx::backend(); }
    std::string   tag()     const { return Net::tag(); }
    cv::util::any params()  const { return { desc }; }
    // END(G-API's network parametrization API)

protected:
    detail::ParamDesc desc;
};

/*
* @brief This structure provides functions for generic network type that
* fill inference parameters.
* @see struct Generic
*/
template<>
class Params<cv::gapi::Generic> {
public:
    /** @brief Class constructor.

    Constructs Params based on input information and sets default values for other
    inference description parameters.

    @param tag string tag of the network for which these parameters are intended.
    @param model_path path to model file (.onnx file).
    */
    Params(const std::string& tag, const std::string& model_path)
        : desc{model_path, 0u, 0u, {}, {}, {}, {}, {}, {}, {}, {}, {}, true, {}, {}, {}, false }, m_tag(tag) {}

    void cfgMeanStdDev(const std::string &layer,
                       const cv::Scalar &m,
                       const cv::Scalar &s) {
        desc.generic_mstd[layer] = std::make_pair(m, s);
    }

    void cfgNormalize(const std::string &layer, bool flag) {
        desc.generic_norm[layer] = flag;
    }

    void cfgExecutionProvider(ep::OpenVINO&& ov_ep) {
        desc.execution_provider = std::move(ov_ep);
    }

    void cfgDisableMemPattern() {
        desc.disable_mem_pattern = true;
    }

    // BEGIN(G-API's network parametrization API)
    GBackend      backend() const { return cv::gapi::onnx::backend(); }
    std::string   tag()     const { return m_tag; }
    cv::util::any params()  const { return { desc }; }
    // END(G-API's network parametrization API)
protected:
    detail::ParamDesc desc;
    std::string m_tag;
};

} // namespace onnx
} // namespace gapi
} // namespace cv

#endif // OPENCV_GAPI_INFER_HPP
