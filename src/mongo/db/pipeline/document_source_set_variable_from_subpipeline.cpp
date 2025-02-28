/**
 *    Copyright (C) 2022-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */
#define MONGO_LOGV2_DEFAULT_COMPONENT ::mongo::logv2::LogComponent::kQuery

#include "mongo/db/pipeline/document_source_set_variable_from_subpipeline.h"
#include "mongo/platform/basic.h"
#include <boost/intrusive_ptr.hpp>
#include <boost/optional.hpp>
#include <vector>

#include "mongo/db/exec/document_value/document.h"
#include "mongo/db/pipeline/document_source_set_variable_from_subpipeline_gen.h"
#include "mongo/db/pipeline/expression_context.h"
#include "mongo/db/views/resolved_view.h"
#include "mongo/logv2/log.h"

namespace mongo {

using boost::intrusive_ptr;

constexpr StringData DocumentSourceSetVariableFromSubPipeline::kStageName;

Value DocumentSourceSetVariableFromSubPipeline::serialize(
    boost::optional<ExplainOptions::Verbosity> explain) const {
    const auto var = "$$" + Variables::getBuiltinVariableName(_variableID);
    tassert(625298, "SubPipeline cannot be null during serialization", _subPipeline);
    return Value(DOC(getSourceName()
                     << DOC("setVariable" << var << "pipeline" << _subPipeline->serialize())));
}

DepsTracker::State DocumentSourceSetVariableFromSubPipeline::getDependencies(
    DepsTracker* deps) const {
    // TODO SERVER-63845: change to NOT_SUPPORTED.
    return DepsTracker::State::SEE_NEXT;
}

Pipeline::SourceContainer::iterator DocumentSourceSetVariableFromSubPipeline::doOptimizeAt(
    Pipeline::SourceContainer::iterator itr, Pipeline::SourceContainer* container) {
    tassert(625295, "Iterator mismatch during optimization", *itr == this);
    auto nextStage = std::next(itr);
    if (nextStage == container->end()) {
        return container->end();
    }
    // SetVariableFromSubPipeline can be moved after any shard-only stage that does not
    // reference the $$SEARCH_META variable.

    // Confirm it is a shards only stage.
    if (nextStage->get()->distributedPlanLogic()->shardsStage &&
        !nextStage->get()->distributedPlanLogic()->mergingStage) {

        DepsTracker depsTracker;
        nextStage->get()->getDependencies(&depsTracker);
        // Check if next stage uses $$SEARCH_META.
        if (!depsTracker.hasVariableReferenceTo(std::set<Variables::Id>{_variableID})) {
            std::swap(*itr, *nextStage);
            return std::prev(itr);
        }
    }

    return nextStage;
}

boost::intrusive_ptr<DocumentSource> DocumentSourceSetVariableFromSubPipeline::createFromBson(
    BSONElement elem, const boost::intrusive_ptr<ExpressionContext>& expCtx) {
    uassert(
        ErrorCodes::FailedToParse,
        str::stream()
            << "the $setVariableFromSubPipeline stage specification must be an object, but found "
            << typeName(elem.type()),
        elem.type() == BSONType::Object);

    auto spec = SetVariableFromSubPipelineSpec::parse(IDLParserErrorContext(kStageName),
                                                      elem.embeddedObject());
    const auto searchMetaStr = "$$" + Variables::getBuiltinVariableName(Variables::kSearchMetaId);
    uassert(
        625291,
        str::stream() << "SetVariableFromSubPipeline only allows setting $$SEARCH_META variable,  "
                      << spec.getSetVariable().toString() << " is not allowed.",
        spec.getSetVariable().toString() == searchMetaStr);

    std::unique_ptr<Pipeline, PipelineDeleter> pipeline =
        Pipeline::parse(spec.getPipeline(), expCtx);

    return DocumentSourceSetVariableFromSubPipeline::create(
        expCtx, std::move(pipeline), Variables::kSearchMetaId);
}

intrusive_ptr<DocumentSourceSetVariableFromSubPipeline>
DocumentSourceSetVariableFromSubPipeline::create(
    const boost::intrusive_ptr<ExpressionContext>& expCtx,
    std::unique_ptr<Pipeline, PipelineDeleter> subpipeline,
    Variables::Id varID) {
    uassert(625290,
            str::stream()
                << "SetVariableFromSubPipeline only allows setting $$SEARCH_META variable,  '$$"
                << Variables::getBuiltinVariableName(varID) << "' is not allowed.",
            !Variables::isUserDefinedVariable(varID) && varID == Variables::kSearchMetaId);
    return intrusive_ptr<DocumentSourceSetVariableFromSubPipeline>(
        new DocumentSourceSetVariableFromSubPipeline(expCtx, std::move(subpipeline), varID));
};

DocumentSource::GetNextResult DocumentSourceSetVariableFromSubPipeline::doGetNext() {
    if (_firstCallForInput) {
        auto nextSubPipelineInput = _subPipeline->getNext();
        tassert(625296,
                "No document returned from $SetVariableFromSubPipeline subpipeline ",
                nextSubPipelineInput);
        tassert(625297,
                "Multiple documents returned from $SetVariableFromSubPipeline subpipeline when "
                "only one expected",
                !_subPipeline->getNext());
        pExpCtx->variables.setReservedValue(_variableID, Value(*nextSubPipelineInput), true);
    }
    _firstCallForInput = false;
    return pSource->getNext();
}

}  // namespace mongo
