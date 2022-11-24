using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR;

public class XRPlayerController : MonoBehaviour
{
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
    private CapsuleDirection capsuleDirection = CapsuleDirection.YAxis;

    private InputDevice controller;
    private bool isGrounded = false;
    private bool buttonPressed = false;
    private Rigidbody rigidbodyComponent = null;
    private CapsuleCollider capsuleCollider = null;
    private List<InputDevice> devices = new List<InputDevice>();

    public enum CapsuleDirection
    {
        XAxis,
        YAxis,
        ZAxis
    }

    private void OnEnable()
    {
        rigidbodyComponent = GetComponent<Rigidbody>();
        capsuleCollider = GetComponent<CapsuleCollider>();

        rigidbodyComponent.constraints = RigidbodyConstraints.FreezeRotation;
        capsuleCollider.direction = (int)capsuleDirection;
        capsuleCollider.radius = capsuleRadius;
        capsuleCollider.center = capsuleCenter;
        capsuleCollider.height = capsuleHeight;

    }

    void Start()
    {
        GetDevice();
    }

    private void GetDevice()
    {
        InputDevices.GetDevicesAtXRNode(controllerNode, devices);
    }

    // Update is called once per frame
    void Update()
    {
        if(controller == null)
        {
            GetDevice();
        }

        UpdateMovement();
        UpdateJump(controller);
    }

    private void UpdateMovement()
    {
        Vector2 primary2dValue;

        InputFeatureUsage<Vector2> primary2DValue = CommonUsages.primary2DAxis;

        if (controller.TryGetFeatureValue(primary2DValue, out primary2dValue) && primary2dValue != Vector2.zero)
        {
            var xAxis = primary2dValue.x * speed * Time.deltaTime;
            var zAxis = primary2dValue.y * speed * Time.deltaTime;

            Vector3 right = transform.TransformDirection(Vector3.right);
            Vector3 forward = transform.TransformDirection(Vector3.forward);

            transform.position = right * xAxis;
            transform.position = forward * zAxis;
        }
    }

    private void UpdateJump(InputDevice controller)
    {
        isGrounded = (Physics.Raycast((new Vector2(transform.position.x, transform.position.y + 2.0f)), Vector3.down, 5.0f));

        if (!isGrounded && checkForGroundOnJump)
        {
            return;
        }

        bool buttonValue;

        if (controller.TryGetFeatureValue(CommonUsages.primaryButton, out buttonValue) && buttonValue)
        {
            if (!buttonPressed)
            {
                buttonPressed = true;
                rigidbodyComponent.AddForce(Vector3.up * jumpForce);

            }
            else if (buttonPressed)
            {
                buttonPressed = false;
            }
        }

    }
}
