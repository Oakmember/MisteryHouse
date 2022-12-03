using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PlayerManager : MonoBehaviour
{
    public static PlayerManager instance;

    [SerializeField]
    private bool isTeleport = false;

    [SerializeField]
    private LocomotionSystem locomotionSystem = null;

    [SerializeField]
    private GameObject leftTelportingRay = null;

    [SerializeField]
    private GameObject rightTelportingRay = null;

    [SerializeField]
    private XRPlayerController xrPlayerController = null;

    private TeleportationProvider teleportationProvider = null;

    public static PlayerManager Instance
    {
        get
        {
            return instance;
        }
    }

    private void Awake()
    {
        if (instance != null && instance != this)
        {
            Destroy(this.gameObject);
        }
        instance = this;

        teleportationProvider = locomotionSystem.gameObject.GetComponent<TeleportationProvider>();
    }

    void Start()
    {
        //SetTeleport(isTeleport);
    }

    public void SetTeleport(bool isTeleportParam)
    {
        teleportationProvider.enabled = isTeleportParam;
        xrPlayerController.IsContinousMoving = !isTeleportParam;

        leftTelportingRay.SetActive(isTeleportParam);
        rightTelportingRay.SetActive(isTeleportParam);
    }

   
}
