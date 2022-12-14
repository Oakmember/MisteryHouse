using Shared;
using System;
using System.Collections;
using System.Collections.Generic;
using Unity.XR.CoreUtils;
using UnityEditor;
using UnityEditor.ShaderGraph.Internal;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.XR;
using UnityEngine.XR.Interaction.Toolkit;
using UnityEngine.XR.Interaction.Toolkit.Inputs;

//https://docs.unity3d.com/Manual/xr_input.html

public class XRPlayerController : MonoBehaviour
{

    [SerializeField]
    InputActionProperty runActionProperty;

    public InputActionProperty RunActionProperty
    {
        get => runActionProperty;
        set => runActionProperty = value;
    }

    [SerializeField]
    InputActionProperty jumpActionProperty;

    public InputActionProperty JumpActionProperty
    {
        get => jumpActionProperty;
        set => jumpActionProperty = value;
    }

    [SerializeField]
    InputActionProperty moveActionProperty;

    public InputActionProperty MoveActionProperty
    {
        get => moveActionProperty;
        set => moveActionProperty = value;
    }

    [SerializeField]
    InputActionProperty menuActionProperty;

    public InputActionProperty MenuActionProperty
    {
        get => menuActionProperty;
        set => menuActionProperty = value;
    }

    [SerializeField]
    InputActionProperty hudActionProperty;

    public InputActionProperty HUDActionProperty
    {
        get => hudActionProperty;
        set => hudActionProperty = value;
    }

    [SerializeField]
    InputActionProperty wristMenuActionProperty;

    public InputActionProperty WristMenuActionProperty
    {
        get => wristMenuActionProperty;
        set => wristMenuActionProperty = value;
    }

    private InputAction runInputAction;
    private InputAction jumpInputAction;
    private InputAction moveInputAction;
    private InputAction menuInputAction;
    private InputAction hudInputAction;
    private InputAction wristMenuInputAction;

    [SerializeField]
    private LayerMask groundLayer;

    [SerializeField]
    private LayerMask teleportLayer;

    [SerializeField]
    private LayerMask nonMoveLayer;

    [SerializeField]
    private float speed = 10.0f;

    [SerializeField]
    private float sprintFactor = 2.0f;

    [SerializeField]
    private float jumpForce = 250.0f;

    [SerializeField]
    private XRNode controllerNode = XRNode.LeftHand;

    [SerializeField]
    private bool checkForGroundOnJump = true;

    [SerializeField]
    private Vector3 capsuleCenter = new Vector3(0, 1, 0);

    [SerializeField]
    private float capsuleRadius = 0.3f;

    [SerializeField]
    private float capsuleHeight = 1.6f;

    [SerializeField]
    private GameObject menu = null;

    [SerializeField]
    private GameObject wristMenu = null;

    [SerializeField]
    private GameObject hud = null;

    [SerializeField]
    private Transform head = null;

    [SerializeField]
    private float spawnDistance = 2.0f;

    private PlayerManager playerManager = null;

    private UnityEngine.XR.InputDevice controller;
    private bool isGrounded = false;
    private bool buttonPressed = false;
    private Rigidbody rigidbodyComponent = null;
    private CapsuleCollider capsuleCollider = null;
    private List<UnityEngine.XR.InputDevice> devices = new List<UnityEngine.XR.InputDevice>();

    private CharacterController characterController = null;
    private XROrigin rig = null;
    private float fallingSpeed = 0;
    private float gravity = 10.0f;
    private float heightOffset = 0.2f;
    private bool isRunning = false;
    private bool isContinousMoving = true;
    private bool isCheckingGround = true;
    private bool isNonMovingArea = false;

    public bool IsContinousMoving { 
        get => isContinousMoving;
        set => isContinousMoving = value;
    }

    public bool IsCheckingGround
    {
        get => isCheckingGround;
        set => isCheckingGround = value;
    }

    private void Awake()
    {
        characterController = gameObject.GetComponent<CharacterController>();
        capsuleCollider = gameObject.GetComponent<CapsuleCollider>();
        rig = gameObject.GetComponent<XROrigin>();
        rigidbodyComponent = gameObject.GetComponent<Rigidbody>();
    }

    private void OnEnable()
    {
        runInputAction = runActionProperty.action;
        moveInputAction = moveActionProperty.action;
        jumpInputAction = jumpActionProperty.action;
        menuInputAction = menuActionProperty.action;
        hudInputAction = hudActionProperty.action;
        wristMenuInputAction = wristMenuActionProperty.action;

        runInputAction.performed += OnRun;
        runInputAction.canceled += OnRun;
        jumpInputAction.performed += OnJump;
        menuInputAction.performed += OnMenu;
        hudInputAction.performed += OnHUD;
        wristMenuInputAction.performed += OnWristMenu;
    }

    private void OnHUD(InputAction.CallbackContext obj)
    {
        if (!hud) return;

        hud.SetActive(!hud.activeSelf);

    }

    private void OnMenu(InputAction.CallbackContext obj)
    {
        if (!menu) return;

        menu.SetActive(!menu.activeSelf);
        menu.transform.position = head.position + new Vector3(head.forward.x, 0, head.forward.z).normalized * spawnDistance;
        menu.transform.LookAt(new Vector3(head.position.x, menu.transform.position.y, head.position.z));
        menu.transform.forward *= -1;
    }

    private void OnWristMenu(InputAction.CallbackContext obj)
    {
        if (!wristMenu) return;

        wristMenu.SetActive(!wristMenu.activeSelf);
    }

    private void OnRun(InputAction.CallbackContext obj)
    {
        if (!CheckIfGrounded(groundLayer)) return;

        isRunning = runInputAction.ReadValue<float>() >= 1.0f;
    }

    private void OnJump(InputAction.CallbackContext obj)
    {
        if (!CheckIfGrounded(groundLayer)) return;

        rigidbodyComponent.AddForce(Vector3.up * jumpForce);
    }

    void Start()
    {
        if (PlayerManager.instance)
        {
            playerManager = PlayerManager.instance;
            //playerManager.SetTeleport(CheckIfTeleport(teleportLayer));
        }

        GetDevice();
    }

    private void GetDevice()
    {
        InputDevices.GetDevicesAtXRNode(controllerNode, devices);
    }

    private void FixedUpdate()
    {

        CapsuleFollowHeadset();

        if (!isContinousMoving) return;
        //if (!isNonMovingArea) return;

        Vector2 primary2dValue = moveInputAction.ReadValue<Vector2>();
        
        float xAxis = 0.0f;
        float zAxis = 0.0f;

        if (primary2dValue == Vector2.zero) return;

        if (isRunning)
        {
            xAxis = primary2dValue.x * sprintFactor;
            zAxis = primary2dValue.y * sprintFactor;
        }
        else
        {
            xAxis = primary2dValue.x;
            zAxis = primary2dValue.y;
        }

        Quaternion headYaw = Quaternion.Euler(0, rig.Camera.gameObject.transform.eulerAngles.y, 0);
        Vector3 direction = headYaw * new Vector3(xAxis, 0.0f, zAxis);
        characterController.Move(direction * speed * Time.deltaTime);

        if (CheckIfGrounded(groundLayer))
        {
            fallingSpeed = 0;
        }
        else
        {
            fallingSpeed += gravity * Time.fixedDeltaTime;
        }

        characterController.Move(Vector3.down * fallingSpeed * Time.fixedDeltaTime);
    }

    private bool CheckIfGrounded(LayerMask groundlayerParam)
    {
        Vector3 rayStart = transform.TransformPoint(characterController.center);
        float rayLength = characterController.center.y + 0.01f;
        bool hasHit = Physics.SphereCast(rayStart, characterController.radius, Vector3.down, out RaycastHit hitInfo, rayLength, groundlayerParam);
        return hasHit;
    }

    private int CheckWhatLayer()
    {
        Vector3 rayStart = transform.TransformPoint(characterController.center + new Vector3(0,0,1));
        float rayLength = characterController.center.y + 0.01f;
        bool hasHit = Physics.SphereCast(rayStart, characterController.radius, Vector3.down, out RaycastHit hitInfo, rayLength);
        //Debug.Log(hitInfo.transform.gameObject.layer);
        if (!hasHit) return -1;
        GameObject hitGameObject = hitInfo.transform.gameObject;
        if (!hitGameObject) return -1;
      
        return hitGameObject.layer;
    }

    private void CapsuleFollowHeadset()
    {
        characterController.height = rig.CameraInOriginSpaceHeight + heightOffset;
        //capsuleCollider.height = rig.CameraInOriginSpaceHeight + heightOffset;
        Vector3 capsuleCenter = transform.InverseTransformPoint(rig.Camera.gameObject.transform.position);
        characterController.center = new Vector3(capsuleCenter.x, characterController.height / 2 + characterController.skinWidth, capsuleCenter.z);
        //capsuleCollider.center = new Vector3(capsuleCenter.x, characterController.height / 2 + characterController.skinWidth, capsuleCenter.z);
    }

    void Update()
    { //CheckWhatLayer().value == LayerMask.NameToLayer(Consts.teleport)
        //playerManager.SetTeleport();
        SetLayer(CheckWhatLayer());

        if (controller == null)
        {
            GetDevice();
        }
    }

    private void SetLayer(int layerMaskParam)
    {

        if (layerMaskParam == LayerMask.NameToLayer(Consts.teleportLayer))
        {
            //Debug.Log("teleportLayer");
            playerManager.SetTeleport(true);
        }
        else if (layerMaskParam == LayerMask.NameToLayer(Consts.anchorLayer))
        {
            //Debug.Log("anchor");
            playerManager.SetTeleport(true);
        }
        else if (layerMaskParam == LayerMask.NameToLayer(Consts.nonMoveLayer))
        {
            //Debug.Log("nonMoveLayer");
            playerManager.SetTeleport(true);
        }
        else if (layerMaskParam == LayerMask.NameToLayer(Consts.defaultLayer))
        {
            //Debug.Log("default");
            playerManager.SetTeleport(false);
        }
        else
        {
            Debug.Log("Non Valid");
        }
    }

    private void OnDisable()
    {
        runInputAction.performed -= OnRun;
        runInputAction.canceled -= OnRun;
        jumpInputAction.performed -= OnJump;
        menuInputAction.performed -= OnMenu;
        hudInputAction.performed -= OnHUD;
        wristMenuInputAction.performed -= OnWristMenu;
    }
}
