#ifndef AETHER_TEST_PERMISSION_FIXTURE_H
#define AETHER_TEST_PERMISSION_FIXTURE_H
#include "aether/core/permission.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)
#define OK(expr) CHECK((expr) == AETHER_STATUS_OK)

typedef struct PermissionFixture {
    AetherWorkspaceRegistry* workspaces;
    AetherRuntimeRegistry* runtimes;
    AetherSessionRegistry* sessions;
    AetherPermissionRegistry* permissions;
    AetherPermissionInput input;
    AetherSessionId second_session;
} PermissionFixture;

static inline AetherStringView text(const char* s)
{
    return (AetherStringView){s, strlen(s)};
}

static inline PermissionFixture fixture(AetherPermissionLimits limits)
{
    PermissionFixture f = {0};
    AetherWorkspaceId workspace;
    AetherProviderId provider;
    AetherRuntimeId runtime;
    AetherAgentConfigId agent;
    AetherSessionId session;
    OK(aether_workspace_registry_create(2U, 2U, &f.workspaces));
    OK(aether_workspace_create(f.workspaces,
        (AetherWorkspaceInput){text("W"), text("C:/w")}, &workspace));
    OK(aether_runtime_registry_create(2U, 2U, &f.runtimes));
    OK(aether_runtime_provider_register(f.runtimes, text("P"), text("1"), &provider));
    OK(aether_runtime_register(f.runtimes, provider, &runtime));
    OK(aether_runtime_update(f.runtimes, runtime, 1U, AETHER_RUNTIME_READY,
                             (AetherRuntimeCapabilities){0}));
    OK(aether_agent_config_create(f.workspaces,
        (AetherAgentConfigInput){text("A"), provider, text("M")}, &agent));
    OK(aether_session_registry_create(2U, f.workspaces, f.runtimes, &f.sessions));
    OK(aether_session_create(f.sessions, workspace, agent, runtime, 1U, &session));
    OK(aether_session_initialize(f.sessions, session));
    OK(aether_session_create(f.sessions, workspace, agent, runtime, 1U,
                             &f.second_session));
    OK(aether_session_initialize(f.sessions, f.second_session));
    OK(aether_permission_registry_create(limits, f.workspaces, f.sessions,
                                         f.runtimes, &f.permissions));
    f.input.request_id = aether_id_next();
    f.input.scope = (AetherPermissionScope){workspace, session, runtime, provider,
        aether_id_next(), aether_id_next(), text("shell"), text("run")};
    f.input.runtime_generation = 1U;
    f.input.level = AETHER_PERMISSION_LEVEL_EXEC;
    f.input.arguments = text("command=build");
    f.input.capability_supported = true;
    return f;
}

static inline void dispose(PermissionFixture* f)
{
    OK(aether_permission_registry_destroy(&f->permissions));
    OK(aether_session_registry_destroy(&f->sessions));
    OK(aether_runtime_registry_destroy(&f->runtimes));
    OK(aether_workspace_registry_destroy(&f->workspaces));
}

static inline AetherPermissionResult evaluate(PermissionFixture* f)
{
    AetherPermissionResult result;
    OK(aether_permission_evaluate(f->permissions, f->input, &result));
    return result;
}

static inline void mode(PermissionFixture* f, AetherPermissionMode value)
{
    OK(aether_permission_set_mode(f->permissions, f->input.scope.workspace_id,
                                  (AetherSessionId){0}, value));
}

static inline AetherPermissionId begin(PermissionFixture* f)
{
    AetherPermissionResult result;
    OK(aether_permission_begin(f->permissions, f->input, 10U, 1000U, &result));
    CHECK(result.decision == AETHER_PERMISSION_ASK);
    CHECK(result.permission_id.id.value != 0U);
    return result.permission_id;
}

static inline AetherPermissionRule rule(AetherPermissionDecision decision)
{
    AetherPermissionRule result = {0};
    result.rule_id = aether_id_next();
    result.decision = decision;
    return result;
}
#endif
