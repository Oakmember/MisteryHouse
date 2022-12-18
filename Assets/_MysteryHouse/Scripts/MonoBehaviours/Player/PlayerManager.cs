using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;
using static UnityEditor.Rendering.ShadowCascadeGUI;

public class PlayerManager : MonoBehaviour
{
    public static PlayerManager instance;

    [SerializeField]
    private bool isTeleport = false;

    [SerializeField]
    private bool isGrabRay = false;

    [SerializeField]
    private LocomotionSystem locomotionSystem = null;

    [SerializeField]
    private GameObject leftTelportingRay = null;

    [SerializeField]
    private GameObject rightTelportingRay = null;

    [SerializeField]
    private XRPlayerController xrPlayerController = null;

    [SerializeField]
    private HandStateType handStateTypeLeft = HandStateType.None;

    [SerializeField]
    private HandStateType handStateTypeRight = HandStateType.None;

    [SerializeField]
    private GameObject leftGrab = null;

    [SerializeField]
    private GameObject rightGrab = null;

    [SerializeField]
    private GameObject leftGrabRay = null;

    [SerializeField]
    private GameObject rightGrabRay = null;

    [SerializeField]
    private XRDirectInteractor leftDirectGrab = null;

    [SerializeField]
    private XRDirectInteractor rightDirectGrab = null;

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

    private void Start()
    {
        SetGrabRay(isGrabRay);
    }

    //private void Update()
    //{
    //    //leftGrabRay.SetActive(leftDirectGrab.interactablesSelected.Count == 0);
    //    //rightGrabRay.SetActive(rightDirectGrab.interactablesSelected.Count == 0);


    //}

    public void SetGrabRay(bool isGrabRayActivedParam)
    {
        leftGrabRay.SetActive(isGrabRayActivedParam);
        rightGrabRay.SetActive(isGrabRayActivedParam);
        leftGrab.SetActive(!isGrabRayActivedParam);
        rightGrab.SetActive(!isGrabRayActivedParam);
    }

    public void SetTeleport(bool isTeleportParam)
    {
        teleportationProvider.enabled = isTeleportParam;
        xrPlayerController.IsContinousMoving = !isTeleportParam;
        xrPlayerController.IsCheckingGround = !isTeleportParam;

        leftTelportingRay.SetActive(isTeleportParam);
        rightTelportingRay.SetActive(isTeleportParam);
    }

    public void SetLeftHandState(HandStateType handStateParam)
    {
        handStateTypeLeft = handStateParam;
    }

    public void SetRightHandState(HandStateType handStateParam)
    {
        handStateTypeRight = handStateParam;
    }
}
