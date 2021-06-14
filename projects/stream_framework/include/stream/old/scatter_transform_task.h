#pragma once
#include "task.h"
#include <functional>
#include <span>
#include <vector>

namespace tasking {

template <typename Input, typename Output>
class ScatterTransformTask : public Task<Output> {
public:
    void connect(Task<Output>* pTask, unsigned port);

    void execute() final;

private:
    friend class TaskPool;
    using Kernel = std::function<void(std::span<const Input>, std::span<Output>, std::span<uint32_t>)>;
    ScatterTransformTask(Kernel&& kernel, unsigned numOutputs);

private:
    Kernel m_kernel;
    std::vector<Task<Output>*> m_pOutputs;
};

template <typename Input, typename Output>
inline ScatterTransformTask<Input, Output>::ScatterTransformTask(Kernel&& kernel, unsigned numOutputs)
    : m_kernel(std::move(kernel))
    , m_outputs(static_cast<size_t>(numOutputs))
{
}

template <typename Input, typename Output>
inline void ScatterTransformTask<Input, Output>::connect(Task<Output>* pTask, unsigned port)
{
    m_outputs[port] = pTask;
}

template <typename Input, typename Output>
inline void ScatterTransformTask<Input, Output>::execute()
{
    assert(m_pOutput != nullptr);
    for (std::span<const T> inputBlock : m_inputStream.consume()) {
        std::vector<uint32_t> output(static_cast<size_t>(inputBlock.size()));
        std::vector<Output> outputNodeIndices(static_cast<size_t>(inputBlock.size()));
        m_kernel(inputBlock, output, outputNodeIndices);

        for (uint32_t outputNode = 0; outputNode < static_cast<uint32_t>(m_outputs.size()); outputNode++) {
            std::vector<Output> out;
            out.reserve(output.size() * 2 / m_outputs.size());
            for (size_t i = 0; i < output.size(); i++) {
                if (outputNodeIndices[i] == outputNode)
                    out.push_back(output[i]);
            }

            if (!out.empty())
                m_outputs[outputNode].push(std::move(out));
        }
    }
}

}