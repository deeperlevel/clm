#pragma once

#include "test.hpp"
#include "../clm/container.hpp"

static clm_id clm_container_test_dependency_id;
static clm_id clm_container_test_parent_dependency_id;
static clm_id clm_container_test_transient_next_id;
static clm_int clm_container_test_dependency_constructions;
static clm_int clm_container_test_parent_constructions;
static clm_int clm_container_test_transient_constructions;
static clm_id clm_container_test_invoke_result;

static clm_int clm_container_test_constructor(
	const clm_id service_id,
	const clm_id context_id,
	clm_id* out_instance_id)
{
	if (out_instance_id == nullptr)
		return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
	clm_id instance_id = context_id;
	if (service_id == clm_container_test_dependency_id) {
		++clm_container_test_dependency_constructions;
	} else if (service_id == clm_container_test_parent_dependency_id) {
		clm_id dependency_instance_id;
		if (clm_container_resolve(clm_container_test_dependency_id,
				&dependency_instance_id) != CLM_CONTAINER_STATUS_SUCCESS)
			return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
		clm_bpa_block_node* block;
		if (clm_bpa_create_sized(instance_id, clm::bpa::CLM_BPA_TYPE_STRUCT,
			sizeof(dependency_instance_id)) != clm::bpa::CLM_BPA_SUCCESS)
			return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
		block = clm_bpa_find_block(instance_id);
		if (block == nullptr || block->data.empty()) {
			(void)clm_bpa_delete(instance_id);
			return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
		}
		std::memcpy(block->data.data(), &dependency_instance_id,
			sizeof(dependency_instance_id));
		++clm_container_test_parent_constructions;
		*out_instance_id = instance_id;
		return CLM_CONTAINER_STATUS_SUCCESS;
	} else if (service_id == 94003) {
		instance_id = clm_container_test_transient_next_id++;
		++clm_container_test_transient_constructions;
	} else {
		return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
	}
	if (clm_bpa_create_sized(instance_id, clm::bpa::CLM_BPA_TYPE_STRUCT,
		sizeof(service_id)) != clm::bpa::CLM_BPA_SUCCESS)
		return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
	*out_instance_id = instance_id;
	return CLM_CONTAINER_STATUS_SUCCESS;
}

static clm_int clm_container_test_invoke(const clm_id context_id)
{
	(void)context_id;
	return clm_container_resolve(clm_container_test_dependency_id,
		&clm_container_test_invoke_result);
}

static int test_clm_container()
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Container IoC");
	const clm_id dependency_id = 94001;
	const clm_id parent_id = 94002;
	const clm_id transient_id = 94003;
	const clm_id missing_service_id = 94004;
	const clm_id scoped_service_id = 94005;
	const clm_id dependency_instance_id = 94020;
	const clm_id parent_instance_id = 94021;
	const clm_id required_dependencies[] = {dependency_id};
	const clm_id missing_dependencies[] = {93999};
	clm_id parent_resolved_id = 0;
	clm_id first_transient_id = 0;
	clm_id second_transient_id = 0;
	clm_id stored_dependency_id = 0;
	clm_container_test_dependency_id = dependency_id;
	clm_container_test_parent_dependency_id = parent_id;
	clm_container_test_transient_next_id = 94030;
	clm_container_test_dependency_constructions = 0;
	clm_container_test_parent_constructions = 0;
	clm_container_test_transient_constructions = 0;
	clm_container_test_invoke_result = 0;

	CLM_ASSERT(clm_container_register(dependency_id, nullptr, 0,
		clm_container_test_constructor, CLM_CONTAINER_LIFETIME_SINGLETON,
		dependency_instance_id) == CLM_CONTAINER_REGISTRATION_SUCCESS &&
		clm_container_test_dependency_constructions == 1 &&
		clm_bpa_is_existing(dependency_instance_id),
		"register and eagerly construct a singleton BPA service");
	CLM_ASSERT(clm_container_register(parent_id, required_dependencies, 1,
		clm_container_test_constructor, CLM_CONTAINER_LIFETIME_SINGLETON,
		parent_instance_id) == CLM_CONTAINER_REGISTRATION_SUCCESS &&
		clm_container_test_parent_constructions == 1 &&
		clm_bpa_find_block(parent_instance_id) != nullptr &&
		!clm_bpa_find_block(parent_instance_id)->data.empty() &&
		(std::memcpy(&stored_dependency_id,
			clm_bpa_find_block(parent_instance_id)->data.data(),
			sizeof(stored_dependency_id)),
		stored_dependency_id == dependency_instance_id),
		"validate dependencies and let the constructor resolve them");
	CLM_ASSERT(clm_container_register(parent_id, nullptr, 0,
		clm_container_test_constructor, CLM_CONTAINER_LIFETIME_TRANSIENT, 0) ==
		CLM_CONTAINER_STATUS_ALREADY_REGISTERED &&
		clm_container_register(missing_service_id, missing_dependencies, 1,
			clm_container_test_constructor, CLM_CONTAINER_LIFETIME_TRANSIENT,
			94040) == CLM_CONTAINER_STATUS_MISSING_DEPENDENCY &&
		clm_container_register(scoped_service_id, nullptr, 0,
			clm_container_test_constructor, CLM_CONTAINER_LIFETIME_SCOPED,
			94041) == CLM_CONTAINER_STATUS_UNSUPPORTED_LIFETIME,
		"reject duplicate, missing dependency, and unsupported lifetime");
	CLM_ASSERT(clm_container_register(transient_id, required_dependencies, 1,
		clm_container_test_constructor, CLM_CONTAINER_LIFETIME_TRANSIENT, 0) ==
		CLM_CONTAINER_REGISTRATION_SUCCESS &&
		clm_container_test_transient_constructions == 0 &&
		clm_container_resolve(transient_id, &first_transient_id) ==
		CLM_CONTAINER_STATUS_SUCCESS &&
		clm_container_resolve(transient_id, &second_transient_id) ==
		CLM_CONTAINER_STATUS_SUCCESS &&
		first_transient_id != second_transient_id &&
		clm_container_test_transient_constructions == 2,
		"construct a fresh BPA instance for every transient resolve");
	CLM_ASSERT(clm_container_resolve(parent_id, &parent_resolved_id) ==
		CLM_CONTAINER_STATUS_SUCCESS && parent_resolved_id == parent_instance_id &&
		clm_container_invoke(required_dependencies, 1,
			clm_container_test_invoke, 0) == CLM_CONTAINER_STATUS_SUCCESS &&
		clm_container_test_invoke_result == dependency_instance_id,
		"reuse the singleton and invoke after validation without injection");
	CLM_ASSERT(clm_bpa_delete(first_transient_id) == clm::bpa::CLM_BPA_SUCCESS &&
		clm_bpa_delete(second_transient_id) == clm::bpa::CLM_BPA_SUCCESS &&
		clm_container_unregister(parent_id) == CLM_CONTAINER_UNREGISTER_SUCCESS &&
		!clm_bpa_is_existing(parent_instance_id) &&
		clm_container_unregister(transient_id) == CLM_CONTAINER_UNREGISTER_SUCCESS &&
		clm_container_unregister(dependency_id) == CLM_CONTAINER_UNREGISTER_SUCCESS &&
		!clm_bpa_is_existing(dependency_instance_id),
		"release transient instances and delete owned singletons on unregister");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

