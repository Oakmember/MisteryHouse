using System;
using System.Collections;
using System.Collections.Generic;
using Unity.XR.CoreUtils;
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

    private InputAction runInputAction;
    private InputAction jumpInputAction;
    private InputAction moveInputAction;

    [SerializeField]
    private LayerMask groundLayer;

    [SerializeField]
    private LayerMask teleportLayer;

    [SerializeField]
    private float speed = 10.0f;

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
    private float sprintFactor = 2.0f;

    private PlayerManager playerManager = null;

    private UnityEngine.XR.InputDevice controller;
    private bool isGrounded = false;
    private bool buttonPressed = false;
    private Rigidbody rigidbodyComponent = null;
    private CapsuleCollider capsuleCollider = null;
    private List<UnityEngine.XR.InputDevice> devices = new List<UnityEngine.XR.InputDevice>();

    private CharacterController character = null;
    private XROrigin rig = null;
    private float fallingSpeed = 0;
    private float gravity = 10.0f;
    private float heightOffset = 0.2f;
    private bool isRunning = false;
    private bool isContinousMoving = true;

    public bool IsContinousMoving { 
        get => isContinousMoving;
        set => isContinousMoving = value;
    }

    private void Awake()
    {
        character = gameObject.GetComponent<CharacterController>();
        rig = gameObject.GetComponent<XROrigin>();
        rigidbodyComponent = gameObject.GetComponent<Rigidbody>();
    }

    private void OnEnable()
    {
        runInputAction = runActionProperty.action;
        moveInputAction = moveActionProperty.action;
        jumpInputAction = jumpActionProperty.action;

        runInputAction.performed += OnRun;
        runInputAction.canceled += OnRun;
        jumpInputAction.performed += OnJump;
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
            playerManager.SetTeleport(CheckIfGrounded(teleportLayer));
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
        character.Move(direction * speed * Time.deltaTime);

        if (CheckIfGrounded(groundLayer))
        {
            fallingSpeed = 0;
        }
        else
        {
            fallingSpeed += gravity * Time.fixedDeltaTime;
        }

        character.Move(Vector3.down * fallingSpeed * Time.fixedDeltaTime);
    }

    private bool CheckIfGrounded(LayerMask groundlayerParam)
    {
        Vector3 rayStart = transform.TransformPoint(character.center);
        float rayLength = character.center.y + 0.01f;
        bool hasHit = Physics.SphereCast(rayStart, character.radius, Vector3.down, out RaycastHit hitInfo, rayLength, groundlayerParam);
        return hasHit;
    }

    private void CapsuleFollowHeadset()
    {
        character.height = rig.CameraInOriginSpaceHeight + heightOffset;
        Vector3 capsuleCenter = transform.InverseTransformPoint(rig.Camera.gameObject.transform.position);
        character.center = new Vector3(capsuleCenter.x, character.height / 2 + character.skinWidth, capsuleCenter.z);
    }

    void Update()
    {
        playerManager.SetTeleport(CheckIfGrounded(teleportLayer));

        if (controller == null)
        {
            GetDevice();
        }
    }

    private void OnDisable()
    {
        runInputAction.performed -= OnRun;
        runInputAction.canceled -= OnRun;
        jumpInputAction.performed -= OnJump;
    }
}
