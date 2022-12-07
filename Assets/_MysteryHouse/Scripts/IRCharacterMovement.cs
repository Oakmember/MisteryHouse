using System.Collections;
using System.Collections.Generic;
using Unity.XR.OpenVR;
using UnityEngine;
using UnityEngine.InputSystem;

public class IRCharacterMovement : MonoBehaviour

{

    [SerializeField] LayerMask groundLayers;

    [SerializeField] private float runSpeed = 8f;

    [SerializeField] float jumpHeight = 2f;

    [SerializeField] private InputActionReference jumpActionReference;

    private float gravity = -500f;

    private CharacterController characterController;

    private Vector3 velocity;

    //private bool isGrounded;

    //private bool isGrounded => Physics.Raycast(new Vector2(transform.position.x, transform.position.y + 2.0f), Vector3.down, 2.0f);

    private float horizontalInput;

    private bool isGrounded => Physics.Raycast(

    new Vector2(transform.position.x, transform.position.y + 2.0f),

    Vector3.down, 2.0f);

    // Start is called before the first frame update

    void Start()

    {

        characterController = GetComponent<CharacterController>();

        jumpActionReference.action.performed += OnJump;

    }

    // Update is called once per frame

    void Update()

    {

        horizontalInput = 1;

        //Face Forward

        transform.forward = new Vector3(horizontalInput, 0, Mathf.Abs(horizontalInput) - 1);

        //isGrounded (old)

        //bool isGrounded = Physics.CheckSphere(transform.position, 0.1f, groundLayers, QueryTriggerInteraction.Ignore);

        if (isGrounded && velocity.y < 0)

        {

            velocity.y = 0;

        }

        else

        {

            //Add Gravity

            velocity.y += gravity * Time.deltaTime;

        }

        characterController.Move(new Vector3(horizontalInput * runSpeed, 0, 0) * Time.deltaTime);

        //The following is for using the space bar to jump

        // if (isGrounded && Input.GetButtonDown("jump"))

        // {

        // velocity.y += Mathf.Sqrt(jumpHeight * -2 * gravity);

        // Debug.Log("I Jumped IR");

        // }

        //Vertical Velocity

        characterController.Move(velocity * Time.deltaTime);

    }

    private void OnJump(InputAction.CallbackContext obj)

    {

        if (!isGrounded) return;

        characterController.Move(Vector3.up * jumpHeight);

        Debug.Log("I Jumped JC");

    }

}