# ManagerOffice Environment Dependency

InterviewSim uses a third-party office environment acquired through **Fab**.

The original environment assets are intentionally **not included in this public repository** because they are third-party licensed content.

## InterviewSim Customization

The office environment was customized for InterviewSim by:

- Removing selected props from the original environment
- Placing the Auraa interviewer in the scene
- Configuring the interview camera
- Adjusting the scene for the InterviewSim interview experience

These project-specific modifications were made on top of the licensed third-party environment.

## Fab Asset

InterviewSim Alpha uses the following third-party office environment:

**Manager Office Environment**  
Fab Listing:  
https://www.fab.com/listings/93fb0e6c-0160-4f0a-8ffa-be7f90717641

Developers who want to reproduce the complete InterviewSim interview scene should obtain this asset through Fab using their own account and license.

## Required Environment

To reproduce the complete InterviewSim interview scene from source, developers must obtain the same compatible office environment through Fab and restore it inside the Unreal Engine project.

The main InterviewSim map used during Alpha development is:

```text
/Game/ManagerOffice/MAP/FirstPersonExampleMap
```


## Public Repository Notice

The following third-party environment content is intentionally excluded:

```text
Content/ManagerOffice/
Content/__ExternalActors__/
Content/__ExternalObjects__/
```

where those files are associated with the licensed ManagerOffice environment.

The packaged InterviewSim application may contain cooked versions of the environment as part of the complete application.

Third-party assets remain subject to their respective licenses and terms of use.