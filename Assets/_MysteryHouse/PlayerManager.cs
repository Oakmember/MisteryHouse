using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PlayerManager : MonoBehaviour
{
    [SerializeField]
    private bool isTeleport = false;

    [SerializeField]
    private LocomotionSystem locomotionSystem = null;

    [SerializeField]
    private GameObject leftTelportingRay = null;

    [SerializeField]
    private GameObject rightTelportingRay = null;

    private TeleportationProvider teleportationProvider = null;
    private ActionBasedContinuousMoveProvider actionBasedContinuousMoveProvider = null;

    private void Awake()
    {
        teleportationProvider = locomotionSystem.gameObject.GetComponent<TeleportationProvider>();
        actionBasedContinuousMoveProvider = locomotionSystem.gameObject.GetComponent<ActionBasedContinuousMoveProvider>();
    }

    // Start is called before the first frame update
    void Start()
    {
        SetTeleport(isTeleport);
    }

    private void SetTeleport(bool isTeleportParam)
    {
        teleportationProvider.enabled = isTeleportParam;
        actionBasedContinuousMoveProvider.enabled = !isTeleportParam;

        leftTelportingRay.SetActive(isTeleportParam);
        rightTelportingRay.SetActive(isTeleportParam);
    }

   
}
